/* App-data paths, library refresh, folders, and importing. */

static BOOL create_directory_if_needed(const wchar_t *path) {
    if (CreateDirectoryW(path, NULL)) return TRUE;
    DWORD err = GetLastError();
    return err == ERROR_ALREADY_EXISTS;
}

static BOOL initialize_library_path(void) {
    PWSTR local_app_data = NULL;
    HRESULT hr = SHGetKnownFolderPath(&FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &local_app_data);

    wchar_t base[MAX_PATH * 4] = L"";
    if (SUCCEEDED(hr) && local_app_data) {
        wcsncpy(base, local_app_data, ARRAY_LEN(base) - 1);
        base[ARRAY_LEN(base) - 1] = L'\0';
        CoTaskMemFree(local_app_data);
    } else {
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, (DWORD)ARRAY_LEN(base));
        if (n == 0 || n >= ARRAY_LEN(base)) {
            wcscpy(g_library_root, L"Library unavailable");
            g_library_ready = FALSE;
            return FALSE;
        }
    }

    swprintf(g_app_data_root, ARRAY_LEN(g_app_data_root), L"%ls\\Charter Music Browser", base);
    swprintf(g_library_root, ARRAY_LEN(g_library_root), L"%ls\\Music", g_app_data_root);
    swprintf(g_tools_root, ARRAY_LEN(g_tools_root), L"%ls\\Tools", g_app_data_root);
    swprintf(g_playlists_root, ARRAY_LEN(g_playlists_root), L"%ls\\Playlists", g_app_data_root);
    swprintf(g_playlist_icons_root, ARRAY_LEN(g_playlist_icons_root), L"%ls\\Playlist Icons", g_app_data_root);
    swprintf(g_media_flags_path, ARRAY_LEN(g_media_flags_path), L"%ls\\Library State.cmbstate", g_app_data_root);
    swprintf(g_library_order_path, ARRAY_LEN(g_library_order_path), L"%ls\\Library Order.cmborder", g_app_data_root);
    swprintf(g_discord_config_path, ARRAY_LEN(g_discord_config_path), L"%ls\\Discord Presence.cfg", g_app_data_root);

    if (!create_directory_if_needed(g_app_data_root) ||
        !create_directory_if_needed(g_library_root) ||
        !create_directory_if_needed(g_tools_root) ||
        !create_directory_if_needed(g_playlists_root) ||
        !create_directory_if_needed(g_playlist_icons_root)) {
        wcscpy(g_library_root, L"Library unavailable");
        g_library_ready = FALSE;
        return FALSE;
    }

    refresh_tool_paths();
    g_library_ready = TRUE;
    load_discord_config();
    load_media_flags();
    ensure_liked_playlist_file();
    reload_playlists();
    sync_liked_playlist();
    return TRUE;
}

static int compare_tracks_by_title(const void *a, const void *b) {
    const Track *ta = (const Track *)a;
    const Track *tb = (const Track *)b;
    int r = _wcsicmp(ta->title ? ta->title : L"", tb->title ? tb->title : L"");
    if (r != 0) return r;
    return _wcsicmp(ta->path ? ta->path : L"", tb->path ? tb->path : L"");
}

static void refresh_library(void) {
    if (!g_library_ready) {
        set_status(L"The Charter music library folder is unavailable.");
        return;
    }

    /* Playback owns a path/title snapshot and runs independently. Rebuilding the
       library must never tear down or restart the active decoder. */
    g_current_track_index = (size_t)-1;
    free_tracks();
    rebuild_track_image_list();

    set_status(L"Scanning the Charter music library...");
    UpdateWindow(g_main);

    scan_folder_recursive(g_library_root);
    if (g_track_count > 1) qsort(g_tracks, g_track_count, sizeof(Track), compare_tracks_by_title);
    apply_library_order();
    if (g_page == PAGE_PLAYLIST) load_active_playlist();
    populate_list();

    if (g_playing_path[0]) {
        for (size_t i = 0; i < g_track_count; ++i) {
            if (_wcsicmp(g_tracks[i].path, g_playing_path) == 0) {
                g_current_track_index = i;
                break;
            }
        }
    }

    wchar_t status[1024];
    if (g_track_count == 0) {
        swprintf(status, ARRAY_LEN(status),
                 L"Library is empty. Import media or download it into %ls.", g_library_root);
    } else {
        swprintf(status, ARRAY_LEN(status),
                 L"Library ready. %zu media item%ls found.",
                 g_track_count, g_track_count == 1 ? L"" : L"s");
    }
    set_status(status);
}
static void open_library_folder(void) {
    if (!g_library_ready) {
        set_status(L"The Charter music library folder is unavailable.");
        return;
    }

    HINSTANCE result = ShellExecuteW(g_main, L"open", g_library_root, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        set_status(L"Could not open the Charter music library folder.");
    }
}

static void unique_library_destination(const wchar_t *source, wchar_t *dest, size_t dest_count) {
    const wchar_t *name = base_name(source);
    swprintf(dest, dest_count, L"%ls\\%ls", g_library_root, name);
    if (!file_exists(dest) || _wcsicmp(source, dest) == 0) return;

    wchar_t stem[MAX_PATH * 2];
    wchar_t ext[64] = L"";
    wcsncpy(stem, name, ARRAY_LEN(stem) - 1);
    stem[ARRAY_LEN(stem) - 1] = L'\0';
    wchar_t *dot = wcsrchr(stem, L'.');
    if (dot) {
        wcsncpy(ext, dot, ARRAY_LEN(ext) - 1);
        ext[ARRAY_LEN(ext) - 1] = L'\0';
        *dot = L'\0';
    }
    for (int n = 2; n < 10000; ++n) {
        swprintf(dest, dest_count, L"%ls\\%ls (%d)%ls", g_library_root, stem, n, ext);
        if (!file_exists(dest)) return;
    }
}

static void import_media_files(void) {
    if (!g_library_ready) {
        set_status(L"The Charter media library is unavailable.");
        return;
    }

    IFileOpenDialog *dialog = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IFileOpenDialog, (void **)&dialog);
    if (FAILED(hr) || !dialog) {
        set_status(L"Charter could not open the media picker.");
        return;
    }

    COMDLG_FILTERSPEC types[] = {
        { L"Audio and video", L"*.mp3;*.wav;*.wma;*.m4a;*.aac;*.flac;*.ogg;*.oga;*.opus;*.aiff;*.aif;*.ape;*.weba;*.mka;*.mp4;*.m4v;*.mov;*.wmv;*.avi;*.mkv;*.webm;*.mpeg;*.mpg;*.ogv;*.flv;*.ts;*.m2ts" },
        { L"All files", L"*.*" }
    };
    DWORD options = 0;
    IFileOpenDialog_GetOptions(dialog, &options);
    IFileOpenDialog_SetOptions(dialog, options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);
    IFileOpenDialog_SetFileTypes(dialog, ARRAY_LEN(types), types);
    IFileOpenDialog_SetTitle(dialog, L"Import audio or video into Charter");

    UINT imported = 0, skipped = 0;
    hr = IFileOpenDialog_Show(dialog, g_main);
    if (SUCCEEDED(hr)) {
        IShellItemArray *items = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResults(dialog, &items)) && items) {
            DWORD count = 0;
            IShellItemArray_GetCount(items, &count);
            for (DWORD i = 0; i < count; ++i) {
                IShellItem *item = NULL;
                if (FAILED(IShellItemArray_GetItemAt(items, i, &item)) || !item) continue;
                PWSTR source = NULL;
                if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &source)) && source) {
                    if (is_media_extension(source)) {
                        wchar_t dest[MAX_PATH * 4];
                        unique_library_destination(source, dest, ARRAY_LEN(dest));
                        if (_wcsicmp(source, dest) == 0 || CopyFileW(source, dest, TRUE)) ++imported;
                        else ++skipped;
                    } else {
                        ++skipped;
                    }
                    CoTaskMemFree(source);
                }
                IShellItem_Release(item);
            }
            IShellItemArray_Release(items);
        }
    }
    IFileOpenDialog_Release(dialog);

    if (imported > 0) {
        refresh_library();
        wchar_t status[256];
        swprintf(status, ARRAY_LEN(status), L"Imported %u media file%ls%ls.", imported,
                 imported == 1 ? L"" : L"s", skipped ? L"; some files were skipped" : L"");
        set_status(status);
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        set_status(skipped ? L"No supported media files were imported." : L"Nothing was imported.");
    }
}
