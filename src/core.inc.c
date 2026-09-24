/* Core utilities, media metadata, and thumbnails. */

static int S(int value) {
    return MulDiv(value, (int)g_dpi, 96);
}

static COLORREF blend_color(COLORREF a, COLORREF b, int b_percent) {
    int p = b_percent;
    int q = 100 - p;
    return RGB(
        (GetRValue(a) * q + GetRValue(b) * p) / 100,
        (GetGValue(a) * q + GetGValue(b) * p) / 100,
        (GetBValue(a) * q + GetBValue(b) * p) / 100
    );
}

static wchar_t *dup_wstr(const wchar_t *s) {
    if (!s) return NULL;
    size_t len = wcslen(s);
    wchar_t *copy = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!copy) return NULL;
    memcpy(copy, s, (len + 1) * sizeof(wchar_t));
    return copy;
}

static MediaFlagEntry *find_media_flag(const wchar_t *path) {
    if (!path) return NULL;
    for (size_t i = 0; i < g_media_flag_count; ++i) {
        if (g_media_flags[i].path && _wcsicmp(g_media_flags[i].path, path) == 0)
            return &g_media_flags[i];
    }
    return NULL;
}

static MediaFlagEntry *ensure_media_flag(const wchar_t *path) {
    MediaFlagEntry *entry = find_media_flag(path);
    if (entry) return entry;
    MediaFlagEntry *next = (MediaFlagEntry *)realloc(
        g_media_flags, (g_media_flag_count + 1) * sizeof(MediaFlagEntry));
    if (!next) return NULL;
    g_media_flags = next;
    entry = &g_media_flags[g_media_flag_count];
    ZeroMemory(entry, sizeof(*entry));
    entry->path = dup_wstr(path);
    if (!entry->path) return NULL;
    ++g_media_flag_count;
    return entry;
}

static void free_tracks(void) {
    for (size_t i = 0; i < g_track_count; ++i) {
        free(g_tracks[i].path);
        free(g_tracks[i].title);
        free(g_tracks[i].artist);
        free(g_tracks[i].folder);
    }
    free(g_tracks);
    g_tracks = NULL;
    g_track_count = 0;
    g_track_capacity = 0;
    g_visible_count = 0;
}

static BOOL reserve_track(void) {
    if (g_track_count < g_track_capacity) return TRUE;
    size_t next = g_track_capacity ? g_track_capacity * 2 : 256;
    Track *tmp = (Track *)realloc(g_tracks, next * sizeof(Track));
    if (!tmp) return FALSE;
    g_tracks = tmp;
    g_track_capacity = next;
    return TRUE;
}

static BOOL is_audio_extension(const wchar_t *name) {
    const wchar_t *dot = wcsrchr(name, L'.');
    if (!dot) return FALSE;
    static const wchar_t *exts[] = {
        L".mp3", L".wav", L".wma", L".m4a", L".aac",
        L".flac", L".ogg", L".oga", L".opus", L".mid", L".midi",
        L".aiff", L".aif", L".ape", L".webm", L".weba", L".mka"
    };
    for (size_t i = 0; i < ARRAY_LEN(exts); ++i) {
        if (_wcsicmp(dot, exts[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL is_video_extension(const wchar_t *name) {
    const wchar_t *dot = wcsrchr(name, L'.');
    if (!dot) return FALSE;
    static const wchar_t *exts[] = {
        L".mp4", L".m4v", L".mov", L".wmv", L".avi", L".mkv",
        L".webm", L".mpeg", L".mpg", L".ogv", L".flv", L".ts", L".m2ts"
    };
    for (size_t i = 0; i < ARRAY_LEN(exts); ++i) {
        if (_wcsicmp(dot, exts[i]) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL is_media_extension(const wchar_t *name) {
    return is_audio_extension(name) || is_video_extension(name);
}

static void human_size(ULONGLONG bytes, wchar_t *out, size_t out_count) {
    const wchar_t *unit = L"B";
    double value = (double)bytes;
    if (value >= 1024.0) { value /= 1024.0; unit = L"KB"; }
    if (value >= 1024.0) { value /= 1024.0; unit = L"MB"; }
    if (value >= 1024.0) { value /= 1024.0; unit = L"GB"; }
    if (wcscmp(unit, L"B") == 0)
        swprintf(out, out_count, L"%llu B", bytes);
    else
        swprintf(out, out_count, L"%.1f %ls", value, unit);
}

static const wchar_t *base_name(const wchar_t *path) {
    const wchar_t *slash = wcsrchr(path, L'\\');
    return slash ? slash + 1 : path;
}

static void title_from_filename(const wchar_t *name, wchar_t *out, size_t out_count) {
    wcsncpy(out, name, out_count - 1);
    out[out_count - 1] = L'\0';
    wchar_t *dot = wcsrchr(out, L'.');
    if (dot) *dot = L'\0';
}

static wchar_t *propvariant_dup_text(const PROPVARIANT *pv) {
    if (!pv) return NULL;
    if (pv->vt == VT_LPWSTR && pv->pwszVal && pv->pwszVal[0]) return dup_wstr(pv->pwszVal);
    if (pv->vt == VT_BSTR && pv->bstrVal && pv->bstrVal[0]) return dup_wstr(pv->bstrVal);
    if (pv->vt == (VT_VECTOR | VT_LPWSTR) && pv->calpwstr.cElems > 0) {
        size_t total = 1;
        for (ULONG i = 0; i < pv->calpwstr.cElems; ++i) {
            if (pv->calpwstr.pElems[i]) total += wcslen(pv->calpwstr.pElems[i]) + 3;
        }
        wchar_t *out = (wchar_t *)calloc(total, sizeof(wchar_t));
        if (!out) return NULL;
        for (ULONG i = 0; i < pv->calpwstr.cElems; ++i) {
            const wchar_t *part = pv->calpwstr.pElems[i];
            if (!part || !part[0]) continue;
            if (out[0]) wcscat(out, L", ");
            wcscat(out, part);
        }
        if (out[0]) return out;
        free(out);
    }
    return NULL;
}

static void read_track_metadata(const wchar_t *path, wchar_t **title, wchar_t **artist) {
    if (title) *title = NULL;
    if (artist) *artist = NULL;
    IPropertyStore *store = NULL;
    HRESULT hr = SHGetPropertyStoreFromParsingName(path, NULL, GPS_BESTEFFORT,
                                                    &IID_IPropertyStore, (void **)&store);
    if (FAILED(hr) || !store) return;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (title && SUCCEEDED(IPropertyStore_GetValue(store, &PKEY_Title, &pv))) {
        *title = propvariant_dup_text(&pv);
    }
    PropVariantClear(&pv);

    PropVariantInit(&pv);
    if (artist && SUCCEEDED(IPropertyStore_GetValue(store, &PKEY_Music_Artist, &pv))) {
        *artist = propvariant_dup_text(&pv);
    }
    PropVariantClear(&pv);

    if (artist && !*artist) {
        PropVariantInit(&pv);
        if (SUCCEEDED(IPropertyStore_GetValue(store, &PKEY_Music_AlbumArtist, &pv))) {
            *artist = propvariant_dup_text(&pv);
        }
        PropVariantClear(&pv);
    }

    IPropertyStore_Release(store);
}

static HBITMAP shell_thumbnail_for_path(const wchar_t *path, int size) {
    IShellItemImageFactory *factory = NULL;
    HBITMAP bitmap = NULL;
    HRESULT hr = SHCreateItemFromParsingName(path, NULL, &IID_IShellItemImageFactory,
                                             (void **)&factory);
    if (FAILED(hr) || !factory) return NULL;
    SIZE wanted = { size, size };
    hr = IShellItemImageFactory_GetImage(factory, wanted,
            SIIGBF_RESIZETOFIT | SIIGBF_BIGGERSIZEOK, &bitmap);
    IShellItemImageFactory_Release(factory);
    return SUCCEEDED(hr) ? bitmap : NULL;
}

static HBITMAP prepare_thumbnail_for_dark_ui(HBITMAP source) {
    if (!source) return NULL;
    BITMAP bm;
    ZeroMemory(&bm, sizeof(bm));
    if (!GetObject(source, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return source;
    int width = bm.bmWidth;
    int height = abs(bm.bmHeight);
    size_t pixel_count = (size_t)width * (size_t)height;
    DWORD *pixels = (DWORD *)calloc(pixel_count, sizeof(DWORD));
    if (!pixels) return source;

    BITMAPINFO info;
    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(NULL);
    BOOL got = GetDIBits(screen, source, 0, (UINT)height, pixels, &info, DIB_RGB_COLORS) != 0;
    if (!got) {
        ReleaseDC(NULL, screen);
        free(pixels);
        return source;
    }

    size_t alpha_pixels = 0, translucent_pixels = 0, dark_pixels = 0;
    for (size_t i = 0; i < pixel_count; ++i) {
        BYTE b = (BYTE)(pixels[i] & 0xff);
        BYTE g = (BYTE)((pixels[i] >> 8) & 0xff);
        BYTE r = (BYTE)((pixels[i] >> 16) & 0xff);
        BYTE a = (BYTE)((pixels[i] >> 24) & 0xff);
        if (a) {
            ++alpha_pixels;
            if (a < 255) ++translucent_pixels;
            if ((r + g + b) / 3 < 72 && abs((int)r - (int)g) < 12 &&
                abs((int)g - (int)b) < 12) ++dark_pixels;
        }
    }

    BOOL has_useful_alpha = alpha_pixels > 0 &&
                            (translucent_pixels > 0 || alpha_pixels < pixel_count);
    if (has_useful_alpha && dark_pixels * 100 >= alpha_pixels * 85) {
        for (size_t i = 0; i < pixel_count; ++i) {
            BYTE a = (BYTE)((pixels[i] >> 24) & 0xff);
            BYTE value = (BYTE)MulDiv(235, a, 255);
            pixels[i] = ((DWORD)a << 24) | ((DWORD)value << 16) |
                        ((DWORD)value << 8) | value;
        }
    } else if (alpha_pixels == 0) {
        for (size_t i = 0; i < pixel_count; ++i) pixels[i] |= 0xff000000u;
    }

    void *dib_bits = NULL;
    HBITMAP prepared = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &dib_bits, NULL, 0);
    if (prepared && dib_bits) memcpy(dib_bits, pixels, pixel_count * sizeof(DWORD));
    ReleaseDC(NULL, screen);
    free(pixels);
    if (!prepared) return source;
    DeleteObject(source);
    return prepared;
}

static void thumbnail_edge_average(const DWORD *pixels, int width, int height,
                                   BOOL vertical, BOOL far_edge,
                                   int *out_r, int *out_g, int *out_b,
                                   int *out_deviation) {
    int band = max(1, min(3, vertical ? width : height));
    unsigned long long sum_r = 0, sum_g = 0, sum_b = 0, count = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            BOOL in_band = vertical
                ? (far_edge ? x >= width - band : x < band)
                : (far_edge ? y >= height - band : y < band);
            if (!in_band) continue;
            DWORD pixel = pixels[(size_t)y * (size_t)width + (size_t)x];
            sum_b += pixel & 0xff;
            sum_g += (pixel >> 8) & 0xff;
            sum_r += (pixel >> 16) & 0xff;
            ++count;
        }
    }
    int avg_r = count ? (int)(sum_r / count) : 0;
    int avg_g = count ? (int)(sum_g / count) : 0;
    int avg_b = count ? (int)(sum_b / count) : 0;
    unsigned long long deviation = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            BOOL in_band = vertical
                ? (far_edge ? x >= width - band : x < band)
                : (far_edge ? y >= height - band : y < band);
            if (!in_band) continue;
            DWORD pixel = pixels[(size_t)y * (size_t)width + (size_t)x];
            deviation += (unsigned)abs((int)((pixel >> 16) & 0xff) - avg_r);
            deviation += (unsigned)abs((int)((pixel >> 8) & 0xff) - avg_g);
            deviation += (unsigned)abs((int)(pixel & 0xff) - avg_b);
        }
    }
    if (out_r) *out_r = avg_r;
    if (out_g) *out_g = avg_g;
    if (out_b) *out_b = avg_b;
    if (out_deviation) *out_deviation = count ? (int)(deviation / count) : 255;
}

static BOOL thumbnail_line_matches(const DWORD *pixels, int width, int height,
                                   BOOL vertical, int line,
                                   int bg_r, int bg_g, int bg_b) {
    int length = vertical ? height : width;
    int matches = 0;
    for (int i = 0; i < length; ++i) {
        int x = vertical ? line : i;
        int y = vertical ? i : line;
        DWORD pixel = pixels[(size_t)y * (size_t)width + (size_t)x];
        int distance = abs((int)((pixel >> 16) & 0xff) - bg_r) +
                       abs((int)((pixel >> 8) & 0xff) - bg_g) +
                       abs((int)(pixel & 0xff) - bg_b);
        if (distance <= 54) ++matches;
    }
    return matches * 100 >= length * 88;
}

static void detect_thumbnail_content(const DWORD *pixels, int width, int height,
                                     int *left, int *top, int *right, int *bottom) {
    *left = 0;
    *top = 0;
    *right = width;
    *bottom = height;

    int lr, lg, lb, ld, rr, rg, rb, rd;
    thumbnail_edge_average(pixels, width, height, TRUE, FALSE, &lr, &lg, &lb, &ld);
    thumbnail_edge_average(pixels, width, height, TRUE, TRUE, &rr, &rg, &rb, &rd);
    int side_color_delta = abs(lr - rr) + abs(lg - rg) + abs(lb - rb);
    if (ld <= 48 && rd <= 48 && side_color_delta <= 72) {
        int l = 0, r = width - 1;
        while (l < width / 3 && thumbnail_line_matches(pixels, width, height, TRUE, l, lr, lg, lb)) ++l;
        while (r > width * 2 / 3 && thumbnail_line_matches(pixels, width, height, TRUE, r, rr, rg, rb)) --r;
        int remaining = r - l + 1;
        int removed = width - remaining;
        BOOL center_is_square = abs(remaining - height) <= max(4, height / 7);
        if (remaining > width / 3 && removed >= max(4, width / 12) &&
            (center_is_square || removed >= width / 5)) {
            *left = l;
            *right = r + 1;
        }
    }

    int tr, tg, tb, td, br, bg, bb, bd;
    thumbnail_edge_average(pixels, width, height, FALSE, FALSE, &tr, &tg, &tb, &td);
    thumbnail_edge_average(pixels, width, height, FALSE, TRUE, &br, &bg, &bb, &bd);
    int cap_color_delta = abs(tr - br) + abs(tg - bg) + abs(tb - bb);
    if (td <= 48 && bd <= 48 && cap_color_delta <= 72) {
        int t = 0, b = height - 1;
        while (t < height / 3 && thumbnail_line_matches(pixels, width, height, FALSE, t, tr, tg, tb)) ++t;
        while (b > height * 2 / 3 && thumbnail_line_matches(pixels, width, height, FALSE, b, br, bg, bb)) --b;
        int remaining = b - t + 1;
        int removed = height - remaining;
        int cropped_width = *right - *left;
        BOOL center_is_square = abs(cropped_width - remaining) <= max(4, cropped_width / 7);
        if (remaining > height / 3 && removed >= max(4, height / 12) &&
            (center_is_square || removed >= height / 5)) {
            *top = t;
            *bottom = b + 1;
        }
    }
}

static HBITMAP prepare_media_thumbnail(HBITMAP source, int canvas_width, int canvas_height) {
    if (!source || canvas_width <= 0 || canvas_height <= 0) return NULL;
    BITMAP bm;
    ZeroMemory(&bm, sizeof(bm));
    if (!GetObject(source, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight == 0)
        return NULL;
    int width = bm.bmWidth;
    int height = abs(bm.bmHeight);

    BITMAPINFO source_info;
    ZeroMemory(&source_info, sizeof(source_info));
    source_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    source_info.bmiHeader.biWidth = width;
    source_info.bmiHeader.biHeight = -height;
    source_info.bmiHeader.biPlanes = 1;
    source_info.bmiHeader.biBitCount = 32;
    source_info.bmiHeader.biCompression = BI_RGB;
    size_t pixel_count = (size_t)width * (size_t)height;
    DWORD *pixels = (DWORD *)calloc(pixel_count, sizeof(DWORD));
    if (!pixels) return NULL;
    HDC screen = GetDC(NULL);
    if (!GetDIBits(screen, source, 0, (UINT)height, pixels, &source_info, DIB_RGB_COLORS)) {
        ReleaseDC(NULL, screen);
        free(pixels);
        return NULL;
    }

    int left, top, right, bottom;
    detect_thumbnail_content(pixels, width, height, &left, &top, &right, &bottom);
    int crop_width = max(1, right - left);
    int crop_height = max(1, bottom - top);
    int available_width = max(1, canvas_width - S(4));
    int available_height = max(1, canvas_height - S(4));
    int draw_width, draw_height;
    if ((long long)crop_width * available_height > (long long)crop_height * available_width) {
        draw_width = available_width;
        draw_height = max(1, MulDiv(crop_height, draw_width, crop_width));
    } else {
        draw_height = available_height;
        draw_width = max(1, MulDiv(crop_width, draw_height, crop_height));
    }
    int draw_x = (canvas_width - draw_width) / 2;
    int draw_y = (canvas_height - draw_height) / 2;

    BITMAPINFO canvas_info;
    ZeroMemory(&canvas_info, sizeof(canvas_info));
    canvas_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    canvas_info.bmiHeader.biWidth = canvas_width;
    canvas_info.bmiHeader.biHeight = -canvas_height;
    canvas_info.bmiHeader.biPlanes = 1;
    canvas_info.bmiHeader.biBitCount = 32;
    canvas_info.bmiHeader.biCompression = BI_RGB;
    DWORD *canvas_bits = NULL;
    HBITMAP canvas = CreateDIBSection(screen, &canvas_info, DIB_RGB_COLORS,
                                      (void **)&canvas_bits, NULL, 0);
    if (canvas && canvas_bits) {
        HDC target = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(target, canvas);
        SetStretchBltMode(target, HALFTONE);
        SetBrushOrgEx(target, 0, 0, NULL);
        StretchDIBits(target, draw_x, draw_y, draw_width, draw_height,
                      left, top, crop_width, crop_height,
                      pixels, &source_info, DIB_RGB_COLORS, SRCCOPY);
        SelectObject(target, old);
        DeleteDC(target);
        for (int y = draw_y; y < draw_y + draw_height; ++y) {
            for (int x = draw_x; x < draw_x + draw_width; ++x)
                canvas_bits[(size_t)y * (size_t)canvas_width + (size_t)x] |= 0xff000000u;
        }
    }
    ReleaseDC(NULL, screen);
    free(pixels);
    return canvas;
}

static void rebuild_track_image_list(void) {
    if (g_track_images) {
        ListView_SetImageList(g_list, NULL, LVSIL_SMALL);
        ImageList_Destroy(g_track_images);
        g_track_images = NULL;
    }
    g_track_images = ImageList_Create(S(88), S(52), ILC_COLOR32, 32, 32);
    if (g_list && g_track_images) ListView_SetImageList(g_list, g_track_images, LVSIL_SMALL);
}

static BOOL add_track(const wchar_t *path, ULONGLONG size_bytes) {
    if (!reserve_track()) return FALSE;

    Track t;
    ZeroMemory(&t, sizeof(t));
    t.path = dup_wstr(path);
    if (!t.path) return FALSE;

    wchar_t title_buf[MAX_PATH * 2];
    title_from_filename(base_name(path), title_buf, ARRAY_LEN(title_buf));
    read_track_metadata(path, &t.title, &t.artist);
    if (!t.title || !t.title[0]) {
        free(t.title);
        t.title = dup_wstr(title_buf);
    }
    if (!t.artist || !t.artist[0]) {
        free(t.artist);
        t.artist = dup_wstr(L"Unknown artist");
    }
    if (!t.title || !t.artist) {
        free(t.path);
        free(t.title);
        free(t.artist);
        return FALSE;
    }

    wchar_t folder_buf[MAX_PATH * 4];
    wcsncpy(folder_buf, path, ARRAY_LEN(folder_buf) - 1);
    folder_buf[ARRAY_LEN(folder_buf) - 1] = L'\0';
    wchar_t *last_slash = wcsrchr(folder_buf, L'\\');
    if (last_slash) *last_slash = L'\0';
    t.folder = dup_wstr(folder_buf);
    if (!t.folder) {
        free(t.path);
        free(t.title);
        return FALSE;
    }

    const wchar_t *dot = wcsrchr(path, L'.');
    if (dot && dot[1]) {
        wcsncpy(t.extension, dot + 1, ARRAY_LEN(t.extension) - 1);
        t.extension[ARRAY_LEN(t.extension) - 1] = L'\0';
        CharUpperBuffW(t.extension, (DWORD)wcslen(t.extension));
    }

    t.size_bytes = size_bytes;
    t.is_video = is_video_extension(path);
    MediaFlagEntry *flags = find_media_flag(path);
    t.starred = flags ? flags->starred : FALSE;
    t.liked = flags ? flags->liked : FALSE;
    t.image_index = -1;
    if (g_track_images) {
        HBITMAP thumb = shell_thumbnail_for_path(path, S(176));
        if (thumb) {
            HBITMAP prepared = prepare_media_thumbnail(thumb, S(88), S(52));
            if (prepared) {
                t.image_index = ImageList_Add(g_track_images, prepared, NULL);
                DeleteObject(prepared);
            }
            DeleteObject(thumb);
        }
    }
    g_tracks[g_track_count++] = t;
    return TRUE;
}
