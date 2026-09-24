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

static void rebuild_track_image_list(void) {
    if (g_track_images) {
        ListView_SetImageList(g_list, NULL, LVSIL_SMALL);
        ImageList_Destroy(g_track_images);
        g_track_images = NULL;
    }
    g_track_images = ImageList_Create(S(42), S(42), ILC_COLOR32 | ILC_MASK, 32, 32);
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
        HBITMAP thumb = shell_thumbnail_for_path(path, S(42));
        if (thumb) {
            t.image_index = ImageList_Add(g_track_images, thumb, NULL);
            DeleteObject(thumb);
        }
        if (t.image_index < 0 && (g_icon_musical || g_app_icon)) {
            HICON fallback = t.is_video && g_icon_youtube ? g_icon_youtube :
                             (g_icon_musical ? g_icon_musical : g_app_icon);
            t.image_index = ImageList_AddIcon(g_track_images, fallback);
        }
    }
    g_tracks[g_track_count++] = t;
    return TRUE;
}

