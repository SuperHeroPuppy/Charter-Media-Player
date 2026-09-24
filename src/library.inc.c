/* Library scanning, flags, playlists, and media deletion. */

static void scan_folder_recursive(const wchar_t *folder) {
    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*", folder);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

        wchar_t full[MAX_PATH * 4];
        swprintf(full, ARRAY_LEN(full), L"%ls\\%ls", folder, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_folder_recursive(full);
        } else if (is_media_extension(fd.cFileName)) {
            ULONGLONG size = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            add_track(full, size);
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
}

static BOOL contains_ci(const wchar_t *haystack, const wchar_t *needle) {
    if (!needle || !*needle) return TRUE;
    if (!haystack) return FALSE;

    size_t nlen = wcslen(needle);
    for (const wchar_t *p = haystack; *p; ++p) {
        size_t i = 0;
        while (i < nlen && p[i] && towlower(p[i]) == towlower(needle[i])) ++i;
        if (i == nlen) return TRUE;
    }
    return FALSE;
}

static void update_list_columns(void) {
    if (!g_list) return;
    RECT rc;
    GetClientRect(g_list, &rc);
    int width = rc.right - rc.left;
    if (width <= 0) return;

    int size_w = S(104);
    int artist_w = max(S(220), width * 30 / 100);
    int title_w = max(S(320), width - artist_w - size_w - S(6));

    ListView_SetColumnWidth(g_list, 0, title_w);
    ListView_SetColumnWidth(g_list, 1, artist_w);
    ListView_SetColumnWidth(g_list, 2, size_w);
}

static void free_playlist_tracks(void) {
    for (size_t i = 0; i < g_playlist_track_count; ++i) free(g_playlist_tracks[i]);
    free(g_playlist_tracks);
    g_playlist_tracks = NULL;
    g_playlist_track_count = 0;
}

static BOOL playlist_contains_path(const wchar_t *path) {
    if (!path) return FALSE;
    for (size_t i = 0; i < g_playlist_track_count; ++i) {
        if (_wcsicmp(g_playlist_tracks[i], path) == 0) return TRUE;
    }
    return FALSE;
}

static BOOL write_playlist_file(const PlaylistInfo *pl, wchar_t **items, size_t count) {
    if (!pl || !pl->path[0]) return FALSE;
    HANDLE h = CreateFileW(pl->path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    const WORD bom = 0xFEFF;
    WriteFile(h, &bom, sizeof(bom), &written, NULL);

    wchar_t header[1024];
    swprintf(header, ARRAY_LEN(header),
             L"#CHARTERPLAYLIST\r\n#NAME=%ls\r\n#ICON=%ls\r\n",
             pl->name, pl->icon_path);
    if (!WriteFile(h, header, (DWORD)(wcslen(header) * sizeof(wchar_t)), &written, NULL)) {
        CloseHandle(h);
        return FALSE;
    }

    for (size_t i = 0; i < count; ++i) {
        if (!items[i] || !items[i][0]) continue;
        DWORD bytes = (DWORD)(wcslen(items[i]) * sizeof(wchar_t));
        if (!WriteFile(h, items[i], bytes, &written, NULL)) {
            CloseHandle(h);
            return FALSE;
        }
        const wchar_t eol[] = L"\r\n";
        if (!WriteFile(h, eol, (DWORD)(2 * sizeof(wchar_t)), &written, NULL)) {
            CloseHandle(h);
            return FALSE;
        }
    }
    CloseHandle(h);
    return TRUE;
}

static wchar_t *read_utf16_file(const wchar_t *path, size_t *out_chars) {
    if (out_chars) *out_chars = 0;
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(h);
        return NULL;
    }
    DWORD bytes = (DWORD)size.QuadPart;
    BYTE *raw = (BYTE *)calloc(1, bytes + sizeof(wchar_t) * 2);
    if (!raw) { CloseHandle(h); return NULL; }
    DWORD got = 0;
    BOOL ok = ReadFile(h, raw, bytes, &got, NULL);
    CloseHandle(h);
    if (!ok) { free(raw); return NULL; }
    raw[got] = 0;
    raw[got + 1] = 0;
    wchar_t *text = (wchar_t *)raw;
    size_t chars = got / sizeof(wchar_t);
    if (chars && text[0] == 0xFEFF) {
        memmove(text, text + 1, chars * sizeof(wchar_t));
        --chars;
    }
    if (out_chars) *out_chars = chars;
    return text;
}

static BOOL discord_app_id_valid(const wchar_t *value) {
    if (!value) return FALSE;
    size_t len = wcslen(value);
    if (len < 15 || len > 24) return FALSE;
    for (size_t i = 0; i < len; ++i) {
        if (!iswdigit(value[i])) return FALSE;
    }
    return TRUE;
}

static void update_discord_toggle_label(void) {
    if (g_discord_toggle) {
        SetWindowTextW(g_discord_toggle,
                       g_discord_enabled ? L"Discord presence: On" : L"Discord presence: Off");
        InvalidateRect(g_discord_toggle, NULL, FALSE);
    }
}

static void discord_set_status(const wchar_t *status) {
    wcsncpy(g_discord_status, status ? status : L"", ARRAY_LEN(g_discord_status) - 1);
    g_discord_status[ARRAY_LEN(g_discord_status) - 1] = L'\0';
    if (g_main && g_page == PAGE_SETTINGS) InvalidateRect(g_main, NULL, FALSE);
}

static void discord_mark_dirty(void) {
    InterlockedExchange(&g_discord_dirty, 1);
}

static BOOL save_discord_config(void) {
    if (!g_discord_config_path[0]) return FALSE;
    HANDLE file = CreateFileW(g_discord_config_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    wchar_t text[256];
    swprintf(text, ARRAY_LEN(text), L"enabled=%d\r\napplication_id=%ls\r\n",
             g_discord_enabled ? 1 : 0, g_discord_app_id);
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    BOOL ok = WriteFile(file, &bom, sizeof(bom), &written, NULL) &&
              WriteFile(file, text, (DWORD)(wcslen(text) * sizeof(wchar_t)), &written, NULL);
    CloseHandle(file);
    return ok;
}

static void load_discord_config(void) {
    g_discord_enabled = FALSE;
    g_discord_app_id[0] = L'\0';
    size_t chars = 0;
    wchar_t *text = read_utf16_file(g_discord_config_path, &chars);
    if (text) {
        size_t start = 0;
        for (size_t i = 0; i <= chars; ++i) {
            if (i == chars || text[i] == L'\r' || text[i] == L'\n' || text[i] == L'\0') {
                text[i] = L'\0';
                wchar_t *line = text + start;
                if (_wcsnicmp(line, L"enabled=", 8) == 0)
                    g_discord_enabled = wcstol(line + 8, NULL, 10) != 0;
                else if (_wcsnicmp(line, L"application_id=", 15) == 0) {
                    wcsncpy(g_discord_app_id, line + 15, ARRAY_LEN(g_discord_app_id) - 1);
                    g_discord_app_id[ARRAY_LEN(g_discord_app_id) - 1] = L'\0';
                }
                while (i + 1 < chars && (text[i + 1] == L'\r' || text[i + 1] == L'\n')) ++i;
                start = i + 1;
            }
        }
        free(text);
    }

    if (g_discord_app_id_edit) SetWindowTextW(g_discord_app_id_edit, g_discord_app_id);
    update_discord_toggle_label();
    if (!g_discord_enabled) discord_set_status(L"Disabled");
    else if (!discord_app_id_valid(g_discord_app_id))
        discord_set_status(L"Enter a valid numeric Discord Application ID.");
    else {
        discord_set_status(L"Waiting for the Discord desktop app...");
        discord_mark_dirty();
    }
}

static void free_media_flags(void) {
    for (size_t i = 0; i < g_media_flag_count; ++i) free(g_media_flags[i].path);
    free(g_media_flags);
    g_media_flags = NULL;
    g_media_flag_count = 0;
}

static void load_media_flags(void) {
    free_media_flags();
    if (!g_media_flags_path[0]) return;
    size_t chars = 0;
    wchar_t *text = read_utf16_file(g_media_flags_path, &chars);
    if (!text) return;
    size_t start = 0;
    for (size_t i = 0; i <= chars; ++i) {
        if (i == chars || text[i] == L'\r' || text[i] == L'\n' || text[i] == L'\0') {
            text[i] = L'\0';
            wchar_t *line = text + start;
            if (line[0] && line[1] == L'\t' && line[2]) {
                MediaFlagEntry *entry = ensure_media_flag(line + 2);
                if (entry) {
                    if (line[0] == L'S' || line[0] == L'B') entry->starred = TRUE;
                    if (line[0] == L'H' || line[0] == L'B') entry->liked = TRUE;
                }
            }
            while (i + 1 < chars && (text[i + 1] == L'\r' || text[i + 1] == L'\n')) ++i;
            start = i + 1;
        }
    }
    free(text);
}

static BOOL save_media_flags(void) {
    if (!g_media_flags_path[0]) return FALSE;
    HANDLE h = CreateFileW(g_media_flags_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    DWORD written = 0;
    const WORD bom = 0xFEFF;
    BOOL ok = WriteFile(h, &bom, sizeof(bom), &written, NULL);
    for (size_t i = 0; ok && i < g_media_flag_count; ++i) {
        MediaFlagEntry *entry = &g_media_flags[i];
        if (!entry->path || (!entry->starred && !entry->liked)) continue;
        wchar_t kind = entry->starred && entry->liked ? L'B' : (entry->starred ? L'S' : L'H');
        wchar_t line[MAX_PATH * 4 + 8];
        swprintf(line, ARRAY_LEN(line), L"%lc\t%ls\r\n", kind, entry->path);
        ok = WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, NULL);
    }
    CloseHandle(h);
    return ok;
}

static BOOL ensure_liked_playlist_file(void) {
    if (!g_playlists_root[0]) return FALSE;
    wchar_t path[MAX_PATH * 4];
    swprintf(path, ARRAY_LEN(path), L"%ls\\Liked.cmbpl", g_playlists_root);
    if (file_exists(path)) return TRUE;
    PlaylistInfo liked;
    ZeroMemory(&liked, sizeof(liked));
    wcscpy(liked.name, L"Liked");
    wcsncpy(liked.path, path, ARRAY_LEN(liked.path) - 1);
    liked.is_builtin = TRUE;
    return write_playlist_file(&liked, NULL, 0);
}

static BOOL sync_liked_playlist(void) {
    PlaylistInfo *liked = NULL;
    for (int i = 0; i < g_playlist_count; ++i) {
        if (g_playlists[i].is_builtin) { liked = &g_playlists[i]; break; }
    }
    if (!liked) return FALSE;
    wchar_t **items = NULL;
    size_t count = 0;
    for (size_t i = 0; i < g_media_flag_count; ++i) {
        if (!g_media_flags[i].liked || !g_media_flags[i].path || !file_exists(g_media_flags[i].path)) continue;
        wchar_t **next = (wchar_t **)realloc(items, (count + 1) * sizeof(wchar_t *));
        if (!next) break;
        items = next;
        items[count++] = g_media_flags[i].path;
    }
    BOOL ok = write_playlist_file(liked, items, count);
    free(items);
    return ok;
}

static void load_active_playlist(void) {
    free_playlist_tracks();
    if (g_active_playlist < 0 || g_active_playlist >= g_playlist_count) return;

    size_t wchar_count = 0;
    wchar_t *text = read_utf16_file(g_playlists[g_active_playlist].path, &wchar_count);
    if (!text) return;

    size_t start = 0;
    for (size_t i = 0; i <= wchar_count; ++i) {
        if (i == wchar_count || text[i] == L'\r' || text[i] == L'\n' || text[i] == L'\0') {
            text[i] = L'\0';
            wchar_t *line = text + start;
            if (*line && line[0] != L'#') {
                wchar_t **tmp = (wchar_t **)realloc(g_playlist_tracks,
                                    (g_playlist_track_count + 1) * sizeof(wchar_t *));
                if (tmp) {
                    g_playlist_tracks = tmp;
                    g_playlist_tracks[g_playlist_track_count] = dup_wstr(line);
                    if (g_playlist_tracks[g_playlist_track_count]) ++g_playlist_track_count;
                }
            }
            while (i + 1 < wchar_count && (text[i + 1] == L'\r' || text[i + 1] == L'\n')) ++i;
            start = i + 1;
        }
    }
    free(text);
}

static void read_playlist_metadata(PlaylistInfo *pl) {
    if (!pl) return;
    size_t chars = 0;
    wchar_t *text = read_utf16_file(pl->path, &chars);
    if (!text) return;
    size_t start = 0;
    for (size_t i = 0; i <= chars; ++i) {
        if (i == chars || text[i] == L'\r' || text[i] == L'\n' || text[i] == L'\0') {
            text[i] = L'\0';
            wchar_t *line = text + start;
            if (_wcsnicmp(line, L"#NAME=", 6) == 0 && line[6]) {
                wcsncpy(pl->name, line + 6, ARRAY_LEN(pl->name) - 1);
                pl->name[ARRAY_LEN(pl->name) - 1] = L'\0';
            } else if (_wcsnicmp(line, L"#ICON=", 6) == 0 && line[6]) {
                wcsncpy(pl->icon_path, line + 6, ARRAY_LEN(pl->icon_path) - 1);
                pl->icon_path[ARRAY_LEN(pl->icon_path) - 1] = L'\0';
            }
            while (i + 1 < chars && (text[i + 1] == L'\r' || text[i + 1] == L'\n')) ++i;
            start = i + 1;
        }
    }
    free(text);
    if (pl->icon_path[0] && file_exists(pl->icon_path)) {
        pl->artwork = prepare_thumbnail_for_dark_ui(
            shell_thumbnail_for_path(pl->icon_path, S(36)));
    }
}

static int compare_playlists(const void *a, const void *b) {
    const PlaylistInfo *pa = (const PlaylistInfo *)a;
    const PlaylistInfo *pb = (const PlaylistInfo *)b;
    if (pa->is_builtin != pb->is_builtin) return pa->is_builtin ? -1 : 1;
    return _wcsicmp(pa->name, pb->name);
}

static void reload_playlists(void) {
    wchar_t active_path[MAX_PATH * 4] = L"";
    if (g_active_playlist >= 0 && g_active_playlist < g_playlist_count)
        wcsncpy(active_path, g_playlists[g_active_playlist].path, ARRAY_LEN(active_path) - 1);
    for (int i = 0; i < g_playlist_count; ++i) {
        if (g_playlists[i].artwork) DeleteObject(g_playlists[i].artwork);
        g_playlists[i].artwork = NULL;
    }
    g_playlist_count = 0;
    if (g_playlist_list) SendMessageW(g_playlist_list, LB_RESETCONTENT, 0, 0);
    if (!g_playlists_root[0]) return;

    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*.cmbpl", g_playlists_root);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (g_playlist_count >= (int)ARRAY_LEN(g_playlists)) break;
            PlaylistInfo *pl = &g_playlists[g_playlist_count];
            ZeroMemory(pl, sizeof(*pl));
            wcsncpy(pl->name, fd.cFileName, ARRAY_LEN(pl->name) - 1);
            wchar_t *dot = wcsrchr(pl->name, L'.');
            if (dot) *dot = L'\0';
            swprintf(pl->path, ARRAY_LEN(pl->path), L"%ls\\%ls", g_playlists_root, fd.cFileName);
            pl->is_builtin = _wcsicmp(fd.cFileName, L"Liked.cmbpl") == 0;
            read_playlist_metadata(pl);
            ++g_playlist_count;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    if (g_playlist_count > 1)
        qsort(g_playlists, g_playlist_count, sizeof(PlaylistInfo), compare_playlists);
    if (g_playlist_list) {
        for (int i = 0; i < g_playlist_count; ++i)
            SendMessageW(g_playlist_list, LB_ADDSTRING, 0, (LPARAM)g_playlists[i].name);
    }

    if (active_path[0]) {
        g_active_playlist = -1;
        for (int i = 0; i < g_playlist_count; ++i) {
            if (_wcsicmp(g_playlists[i].path, active_path) == 0) { g_active_playlist = i; break; }
        }
    }
    if (g_active_playlist >= g_playlist_count) g_active_playlist = g_playlist_count - 1;
    if (g_active_playlist >= 0 && g_playlist_list) {
        SendMessageW(g_playlist_list, LB_SETCURSEL, g_active_playlist, 0);
        load_active_playlist();
    } else {
        free_playlist_tracks();
    }
}

static void sanitize_filename_component(const wchar_t *name, wchar_t *out, size_t out_count) {
    size_t j = 0;
    for (size_t i = 0; name && name[i] && j + 1 < out_count; ++i) {
        wchar_t c = name[i];
        if (wcschr(L"<>:\\/*?\"|", c)) c = L'_';
        out[j++] = c;
    }
    while (j > 0 && (out[j - 1] == L' ' || out[j - 1] == L'.')) --j;
    out[j] = L'\0';
    if (!out[0]) wcsncpy(out, L"Playlist", out_count - 1);
}

static BOOL copy_playlist_icon(const wchar_t *source, wchar_t *dest, size_t dest_count) {
    if (!source || !source[0] || !file_exists(source) || !g_playlist_icons_root[0]) {
        if (dest && dest_count) dest[0] = L'\0';
        return TRUE;
    }
    const wchar_t *ext = wcsrchr(source, L'.');
    if (!ext || wcslen(ext) > 10) ext = L".img";
    ULONGLONG stamp = GetTickCount64();
    swprintf(dest, dest_count, L"%ls\\playlist-%llu%ls", g_playlist_icons_root, stamp, ext);
    return CopyFileW(source, dest, FALSE);
}

static int create_new_playlist_named(const wchar_t *name, const wchar_t *icon_source) {
    if (!g_playlists_root[0] || !name || !name[0]) return -1;
    wchar_t safe[128];
    sanitize_filename_component(name, safe, ARRAY_LEN(safe));

    PlaylistInfo temp;
    ZeroMemory(&temp, sizeof(temp));
    wcsncpy(temp.name, name, ARRAY_LEN(temp.name) - 1);
    if (icon_source && icon_source[0] && !copy_playlist_icon(icon_source, temp.icon_path, ARRAY_LEN(temp.icon_path))) {
        return -1;
    }

    for (int n = 1; n < 1000; ++n) {
        if (n == 1) swprintf(temp.path, ARRAY_LEN(temp.path), L"%ls\\%ls.cmbpl", g_playlists_root, safe);
        else swprintf(temp.path, ARRAY_LEN(temp.path), L"%ls\\%ls (%d).cmbpl", g_playlists_root, safe, n);
        if (GetFileAttributesW(temp.path) == INVALID_FILE_ATTRIBUTES) break;
        temp.path[0] = L'\0';
    }
    if (!temp.path[0] || !write_playlist_file(&temp, NULL, 0)) return -1;

    reload_playlists();
    for (int i = 0; i < g_playlist_count; ++i) {
        if (_wcsicmp(g_playlists[i].path, temp.path) == 0 || _wcsicmp(g_playlists[i].name, name) == 0) {
            g_active_playlist = i;
            if (g_playlist_list) SendMessageW(g_playlist_list, LB_SETCURSEL, i, 0);
            load_active_playlist();
            return i;
        }
    }
    return -1;
}

static BOOL update_playlist_details(int index, const wchar_t *name, const wchar_t *icon_source) {
    if (index < 0 || index >= g_playlist_count || !name || !name[0]) return FALSE;
    PlaylistInfo *pl = &g_playlists[index];
    wcsncpy(pl->name, name, ARRAY_LEN(pl->name) - 1);
    pl->name[ARRAY_LEN(pl->name) - 1] = L'\0';
    if (icon_source && icon_source[0]) {
        wchar_t copied[MAX_PATH * 4] = L"";
        if (!copy_playlist_icon(icon_source, copied, ARRAY_LEN(copied))) return FALSE;
        if (pl->icon_path[0] && _wcsicmp(pl->icon_path, copied) != 0) DeleteFileW(pl->icon_path);
        wcsncpy(pl->icon_path, copied, ARRAY_LEN(pl->icon_path) - 1);
        pl->icon_path[ARRAY_LEN(pl->icon_path) - 1] = L'\0';
    }
    load_active_playlist();
    BOOL ok = write_playlist_file(pl, g_playlist_tracks, g_playlist_track_count);
    reload_playlists();
    return ok;
}

static BOOL add_path_to_playlist(int playlist_index, const wchar_t *path) {
    if (playlist_index < 0 || playlist_index >= g_playlist_count || !path) return FALSE;
    int previous = g_active_playlist;
    g_active_playlist = playlist_index;
    load_active_playlist();
    if (playlist_contains_path(path)) {
        if (previous != playlist_index) {
            g_active_playlist = previous;
            load_active_playlist();
        }
        return TRUE;
    }
    wchar_t **tmp = (wchar_t **)realloc(g_playlist_tracks,
                         (g_playlist_track_count + 1) * sizeof(wchar_t *));
    if (!tmp) return FALSE;
    g_playlist_tracks = tmp;
    g_playlist_tracks[g_playlist_track_count] = dup_wstr(path);
    if (!g_playlist_tracks[g_playlist_track_count]) return FALSE;
    ++g_playlist_track_count;
    BOOL ok = write_playlist_file(&g_playlists[playlist_index],
                                   g_playlist_tracks, g_playlist_track_count);
    if (previous != playlist_index) {
        g_active_playlist = previous;
        load_active_playlist();
    }
    return ok;
}

static BOOL remove_path_from_active_playlist(const wchar_t *path) {
    if (g_active_playlist < 0 || g_active_playlist >= g_playlist_count || !path) return FALSE;
    load_active_playlist();
    size_t out = 0;
    for (size_t i = 0; i < g_playlist_track_count; ++i) {
        if (_wcsicmp(g_playlist_tracks[i], path) == 0) {
            free(g_playlist_tracks[i]);
            continue;
        }
        g_playlist_tracks[out++] = g_playlist_tracks[i];
    }
    g_playlist_track_count = out;
    return write_playlist_file(&g_playlists[g_active_playlist],
                                g_playlist_tracks, g_playlist_track_count);
}

static void show_add_to_playlist_menu(void) {
    Track *t = selected_track();
    if (!t) { set_status(L"Select a track first."); return; }
    if (g_playlist_count == 0 && !show_playlist_editor(-1)) {
        set_status(L"No playlist was created.");
        return;
    }
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (int i = 0; i < g_playlist_count; ++i) {
        AppendMenuW(menu, MF_STRING, 5000 + i, g_playlists[i].name);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, 5999, L"New playlist");
    RECT r;
    GetWindowRect(g_add_playlist, &r);
    UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                                 r.left, r.bottom + S(4), 0, g_main, NULL);
    DestroyMenu(menu);
    if (choice == 5999) {
        if (show_playlist_editor(-1)) {
            int idx = g_active_playlist;
            if (idx >= 0 && add_path_to_playlist(idx, t->path)) {
                set_status(L"Created a playlist and added the track.");
            }
        }
    } else if (choice >= 5000 && choice < 5000 + (UINT)g_playlist_count) {
        int idx = (int)choice - 5000;
        if (add_path_to_playlist(idx, t->path)) {
            wchar_t msg[256];
            swprintf(msg, ARRAY_LEN(msg), L"Added to %ls.", g_playlists[idx].name);
            set_status(msg);
            if (g_page == PAGE_PLAYLIST && idx == g_active_playlist) populate_list();
        }
    }
}

static void populate_list(void) {
    wchar_t query[512] = L"";
    if (g_search) GetWindowTextW(g_search, query, ARRAY_LEN(query));

    SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(g_list);

    int visible = 0;
    for (size_t i = 0; i < g_track_count; ++i) {
        Track *t = &g_tracks[i];
        if (g_page == PAGE_PLAYLIST && !playlist_contains_path(t->path)) continue;
        if (!contains_ci(t->title, query) &&
            !contains_ci(t->artist, query) &&
            !contains_ci(t->extension, query)) {
            continue;
        }

        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
        item.iItem = visible;
        wchar_t display_title[1024];
        swprintf(display_title, ARRAY_LEN(display_title), L"%ls%ls%ls",
                 t->starred ? L"★ " : L"", t->liked ? L"♥ " : L"", t->title);
        item.pszText = display_title;
        item.lParam = (LPARAM)i;
        item.iImage = t->image_index;
        int row = ListView_InsertItem(g_list, &item);
        if (row < 0) continue;

        ListView_SetItemText(g_list, row, 1, t->artist ? t->artist : L"Unknown artist");

        wchar_t size_buf[64];
        human_size(t->size_bytes, size_buf, ARRAY_LEN(size_buf));
        ListView_SetItemText(g_list, row, 2, size_buf);
        ++visible;
    }

    g_visible_count = (size_t)visible;
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_list, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    if (g_main) {
        RECT rc;
        GetClientRect(g_main, &rc);
        InvalidateRect(g_main, &rc, FALSE);
    }
}

static int selected_row(void) {
    return ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
}

static Track *selected_track(void) {
    int row = selected_row();
    if (row < 0) return NULL;

    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return NULL;

    size_t idx = (size_t)item.lParam;
    return idx < g_track_count ? &g_tracks[idx] : NULL;
}

static size_t selected_track_index(void) {
    int row = selected_row();
    if (row < 0) return (size_t)-1;
    LVITEMW item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!ListView_GetItem(g_list, &item)) return (size_t)-1;
    size_t idx = (size_t)item.lParam;
    return idx < g_track_count ? idx : (size_t)-1;
}

static void toggle_selected_star(void) {
    Track *track = selected_track();
    if (!track) { set_status(L"Select a media item first."); return; }
    MediaFlagEntry *entry = ensure_media_flag(track->path);
    if (!entry) { set_status(L"Charter could not save the star state."); return; }
    entry->starred = !entry->starred;
    track->starred = entry->starred;
    save_media_flags();
    populate_list();
    set_status(entry->starred ? L"Starred this media item." : L"Removed the star from this media item.");
}

static void toggle_selected_heart(void) {
    Track *track = selected_track();
    if (!track) { set_status(L"Select a media item first."); return; }
    MediaFlagEntry *entry = ensure_media_flag(track->path);
    if (!entry) { set_status(L"Charter could not update the Liked playlist."); return; }
    entry->liked = !entry->liked;
    track->liked = entry->liked;
    save_media_flags();
    sync_liked_playlist();
    if (g_page == PAGE_PLAYLIST && g_active_playlist >= 0 &&
        g_playlists[g_active_playlist].is_builtin) load_active_playlist();
    populate_list();
    set_status(entry->liked ? L"Added to Liked." : L"Removed from Liked.");
}

static void remove_path_from_all_playlists(const wchar_t *path) {
    int previous = g_active_playlist;
    for (int p = 0; p < g_playlist_count; ++p) {
        g_active_playlist = p;
        load_active_playlist();
        size_t out = 0;
        for (size_t i = 0; i < g_playlist_track_count; ++i) {
            if (_wcsicmp(g_playlist_tracks[i], path) == 0) {
                free(g_playlist_tracks[i]);
                continue;
            }
            g_playlist_tracks[out++] = g_playlist_tracks[i];
        }
        if (out != g_playlist_track_count) {
            g_playlist_track_count = out;
            write_playlist_file(&g_playlists[p], g_playlist_tracks, g_playlist_track_count);
        }
    }
    g_active_playlist = previous;
    load_active_playlist();
}

static void remove_media_flag(const wchar_t *path) {
    for (size_t i = 0; i < g_media_flag_count; ++i) {
        if (!g_media_flags[i].path || _wcsicmp(g_media_flags[i].path, path) != 0) continue;
        free(g_media_flags[i].path);
        if (i + 1 < g_media_flag_count)
            memmove(&g_media_flags[i], &g_media_flags[i + 1],
                    (g_media_flag_count - i - 1) * sizeof(MediaFlagEntry));
        --g_media_flag_count;
        save_media_flags();
        return;
    }
}

static void delete_selected_media(void) {
    Track *track = selected_track();
    if (!track) { set_status(L"Select a media item first."); return; }
    wchar_t path[MAX_PATH * 4];
    wchar_t title[768];
    wcsncpy(path, track->path, ARRAY_LEN(path) - 1);
    path[ARRAY_LEN(path) - 1] = L'\0';
    wcsncpy(title, track->title, ARRAY_LEN(title) - 1);
    title[ARRAY_LEN(title) - 1] = L'\0';
    wchar_t prompt[1200];
    swprintf(prompt, ARRAY_LEN(prompt),
             L"Permanently delete \"%ls\" from the Charter library?\n\nThis removes the media file and its playlist entries.",
             title);
    if (MessageBoxW(g_main, prompt, L"Delete media", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;

    if (g_current_track_index < g_track_count &&
        _wcsicmp(g_tracks[g_current_track_index].path, path) == 0) stop_playback();
    if (!DeleteFileW(path)) {
        wchar_t error[512];
        swprintf(error, ARRAY_LEN(error), L"Could not delete the media file (Windows error %lu).", GetLastError());
        set_status(error);
        return;
    }
    wchar_t sidecar[MAX_PATH * 4];
    wcsncpy(sidecar, path, ARRAY_LEN(sidecar) - 1);
    sidecar[ARRAY_LEN(sidecar) - 1] = L'\0';
    wchar_t *dot = wcsrchr(sidecar, L'.');
    if (dot) wcscpy(dot, L".info.json");
    if (file_exists(sidecar)) DeleteFileW(sidecar);
    remove_path_from_all_playlists(path);
    remove_media_flag(path);
    refresh_library();
    set_status(L"Media deleted from Charter and removed from playlists.");
}

