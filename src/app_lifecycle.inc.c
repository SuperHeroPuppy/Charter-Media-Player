/* App-data paths, library refresh, folders, and importing. */

static BOOL create_directory_if_needed(const wchar_t *path) {
    if (CreateDirectoryW(path, NULL)) return TRUE;
    DWORD err = GetLastError();
    return err == ERROR_ALREADY_EXISTS;
}

static BOOL directory_exists(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static BOOL directory_tree_has_files(const wchar_t *folder) {
    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*", folder);
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) return FALSE;
    BOOL has_files = FALSE;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            has_files = TRUE;
            break;
        }
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            has_files = TRUE;
            break;
        }
        wchar_t child[MAX_PATH * 4];
        swprintf(child, ARRAY_LEN(child), L"%ls\\%ls", folder, fd.cFileName);
        if (directory_tree_has_files(child)) {
            has_files = TRUE;
            break;
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return has_files;
}

static BOOL remove_empty_directory_tree(const wchar_t *folder) {
    if (!directory_exists(folder)) return TRUE;
    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*", folder);
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                FindClose(find);
                return FALSE;
            }
            wchar_t child[MAX_PATH * 4];
            swprintf(child, ARRAY_LEN(child), L"%ls\\%ls", folder, fd.cFileName);
            if (!remove_empty_directory_tree(child)) {
                FindClose(find);
                return FALSE;
            }
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    return RemoveDirectoryW(folder) || GetLastError() == ERROR_PATH_NOT_FOUND;
}

static wchar_t *replace_all_wide(const wchar_t *text, const wchar_t *from,
                                 const wchar_t *to) {
    if (!text || !from || !from[0] || !to) return NULL;
    size_t text_len = wcslen(text);
    size_t from_len = wcslen(from);
    size_t to_len = wcslen(to);
    size_t occurrences = 0;
    const wchar_t *cursor = text;
    while ((cursor = wcsstr(cursor, from)) != NULL) {
        ++occurrences;
        cursor += from_len;
    }
    if (!occurrences) return dup_wstr(text);

    size_t output_len = text_len;
    if (to_len >= from_len)
        output_len += occurrences * (to_len - from_len);
    else
        output_len -= occurrences * (from_len - to_len);
    wchar_t *output = (wchar_t *)calloc(output_len + 1, sizeof(wchar_t));
    if (!output) return NULL;

    const wchar_t *source = text;
    wchar_t *dest = output;
    while ((cursor = wcsstr(source, from)) != NULL) {
        size_t prefix = (size_t)(cursor - source);
        memcpy(dest, source, prefix * sizeof(wchar_t));
        dest += prefix;
        memcpy(dest, to, to_len * sizeof(wchar_t));
        dest += to_len;
        source = cursor + from_len;
    }
    wcscpy(dest, source);
    return output;
}

static BOOL write_utf16_text_file(const wchar_t *path, const wchar_t *text) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    const WORD bom = 0xFEFF;
    DWORD written = 0;
    BOOL ok = WriteFile(file, &bom, sizeof(bom), &written, NULL) &&
              written == sizeof(bom);
    DWORD bytes = (DWORD)(wcslen(text) * sizeof(wchar_t));
    ok = ok && WriteFile(file, text, bytes, &written, NULL) && written == bytes;
    CloseHandle(file);
    return ok;
}

static BOOL migration_reference_file(const wchar_t *name) {
    const wchar_t *extension = wcsrchr(name, L'.');
    return extension && (_wcsicmp(extension, L".cmbpl") == 0 ||
                         _wcsicmp(extension, L".cmbstate") == 0 ||
                         _wcsicmp(extension, L".cmborder") == 0);
}

static BOOL rewrite_migrated_references(const wchar_t *folder) {
    wchar_t pattern[MAX_PATH * 4];
    swprintf(pattern, ARRAY_LEN(pattern), L"%ls\\*", folder);
    WIN32_FIND_DATAW fd;
    HANDLE find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) return TRUE;
    BOOL ok = TRUE;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        wchar_t path[MAX_PATH * 4];
        swprintf(path, ARRAY_LEN(path), L"%ls\\%ls", folder, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!rewrite_migrated_references(path)) ok = FALSE;
        } else if (migration_reference_file(fd.cFileName)) {
            size_t chars = 0;
            wchar_t *text = read_utf16_file(path, &chars);
            if (!text) {
                ok = FALSE;
                continue;
            }
            wchar_t *updated = replace_all_wide(
                text, g_legacy_app_data_root, g_new_app_data_root);
            if (!updated || !write_utf16_text_file(path, updated)) ok = FALSE;
            free(updated);
            free(text);
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
    return ok;
}

static BOOL perform_requested_storage_migration(void) {
    if (!g_storage_migration_requested || !g_using_legacy_storage ||
        !directory_exists(g_legacy_app_data_root))
        return FALSE;

    if (directory_exists(g_new_app_data_root)) {
        if (directory_tree_has_files(g_new_app_data_root) ||
            !remove_empty_directory_tree(g_new_app_data_root))
            return FALSE;
    }
    if (!MoveFileExW(g_legacy_app_data_root, g_new_app_data_root,
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        return FALSE;

    BOOL references_ok = rewrite_migrated_references(g_new_app_data_root);
    g_using_legacy_storage = FALSE;
    g_legacy_storage_available = FALSE;
    wcscpy(g_migration_status, references_ok
        ? L"Migration complete."
        : L"Files moved, but one or more playlist references could not be updated.");
    return TRUE;
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

    swprintf(g_legacy_app_data_root, ARRAY_LEN(g_legacy_app_data_root),
             L"%ls\\Charter Music Browser", base);
    swprintf(g_new_app_data_root, ARRAY_LEN(g_new_app_data_root),
             L"%ls\\Charter Media Player", base);
    g_legacy_storage_available = directory_exists(g_legacy_app_data_root);
    BOOL new_has_files = directory_exists(g_new_app_data_root) &&
                         directory_tree_has_files(g_new_app_data_root);
    g_using_legacy_storage = g_legacy_storage_available && !new_has_files;
    wcsncpy(g_app_data_root,
            g_using_legacy_storage ? g_legacy_app_data_root : g_new_app_data_root,
            ARRAY_LEN(g_app_data_root) - 1);
    g_app_data_root[ARRAY_LEN(g_app_data_root) - 1] = L'\0';

    if (g_using_legacy_storage) {
        wcscpy(g_migration_status,
               L"Legacy Charter Music Browser storage is active. Migrate it to the new Charter Media Player folder.");
    } else if (g_legacy_storage_available) {
        wcscpy(g_migration_status,
               L"Both legacy and new storage contain files. Automatic migration is disabled to prevent overwriting data.");
    } else {
        wcscpy(g_migration_status,
               L"No legacy Charter Music Browser storage was found. This installation already uses the new location.");
    }
    swprintf(g_library_root, ARRAY_LEN(g_library_root), L"%ls\\Music", g_app_data_root);
    swprintf(g_tools_root, ARRAY_LEN(g_tools_root), L"%ls\\Tools", g_app_data_root);
    swprintf(g_playlists_root, ARRAY_LEN(g_playlists_root), L"%ls\\Playlists", g_app_data_root);
    swprintf(g_playlist_icons_root, ARRAY_LEN(g_playlist_icons_root), L"%ls\\Playlist Icons", g_app_data_root);
    swprintf(g_media_flags_path, ARRAY_LEN(g_media_flags_path), L"%ls\\Library State.cmbstate", g_app_data_root);
    swprintf(g_library_order_path, ARRAY_LEN(g_library_order_path), L"%ls\\Library Order.cmborder", g_app_data_root);
    swprintf(g_discord_config_path, ARRAY_LEN(g_discord_config_path), L"%ls\\Discord Presence.cfg", g_app_data_root);
    swprintf(g_player_config_path, ARRAY_LEN(g_player_config_path), L"%ls\\Player Settings.cfg", g_app_data_root);

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
    load_player_preferences();
    slider_set_value(g_volume, g_volume_percent);
    enumerate_audio_outputs();
    apply_player_volume();
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

    /* Invalidate results from an older refresh before any Track storage moves. */
    InterlockedIncrement(&g_library_load_generation);

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

    start_library_details_loading();

    wchar_t status[1024];
    if (g_track_count == 0) {
        swprintf(status, ARRAY_LEN(status),
                 L"Library is empty. Import media or download it into %ls.", g_library_root);
    } else if (g_library_details_total > 0) {
        swprintf(status, ARRAY_LEN(status),
                 L"Library ready. %zu media item%ls found; loading details in the background...",
                 g_track_count, g_track_count == 1 ? L"" : L"s");
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
