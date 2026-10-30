/* The installed executable is also the video and direct-audio player.
   Each mode has its own process, so opening a file cannot duplicate the
   library or its notification-area owner. */

static BOOL launch_standalone_player(const wchar_t *path, BOOL video,
                                     HWND parent, LONGLONG position, BOOL paused) {
    if (!path || !path[0] || !file_exists(path)) return FALSE;
    wchar_t executable[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW(NULL, executable, ARRAY_LEN(executable));
    if (!length || length >= ARRAY_LEN(executable)) return FALSE;
    size_t command_chars = wcslen(executable) + wcslen(path) + 128;
    wchar_t *command = (wchar_t *)calloc(command_chars, sizeof(wchar_t));
    if (!command) return FALSE;
    swprintf(command, command_chars, L"\"%ls\" %ls \"%ls\" %llu %lld %d",
             executable, video ? L"--video" : L"--audio", path,
             (unsigned long long)(UINT_PTR)parent, (long long)position,
             paused ? 1 : 0);
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    BOOL started = CreateProcessW(executable, command, NULL, NULL, FALSE,
                                  0, NULL, NULL, &startup, &process);
    free(command);
    if (!started) return FALSE;
    CloseHandle(process.hThread);
    if (parent) {
        g_child_process = process.hProcess;
        g_child_window = NULL;
        g_pending_child_toggle = FALSE;
        g_pending_child_output = FALSE;
    } else {
        CloseHandle(process.hProcess);
    }
    return TRUE;
}

typedef struct ChildWindowSearch {
    DWORD process_id;
    HWND window;
} ChildWindowSearch;

static BOOL CALLBACK find_child_player_window(HWND window, LPARAM context) {
    ChildWindowSearch *search = (ChildWindowSearch *)context;
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id != search->process_id) return TRUE;
    wchar_t class_name[80];
    if (GetClassNameW(window, class_name, ARRAY_LEN(class_name)) &&
        wcscmp(class_name, STANDALONE_CLASS) == 0) {
        search->window = window;
        return FALSE;
    }
    return TRUE;
}

static void stop_standalone_child(void) {
    if (g_child_process && !g_child_window &&
        WaitForSingleObject(g_child_process, 0) == WAIT_TIMEOUT) {
        WaitForInputIdle(g_child_process, 1000);
        ChildWindowSearch search = { GetProcessId(g_child_process), NULL };
        EnumWindows(find_child_player_window, (LPARAM)&search);
        g_child_window = search.window;
    }
    BOOL had_window = g_child_window && IsWindow(g_child_window);
    if (had_window)
        PostMessageW(g_child_window, WM_CLOSE, 0, 0);
    g_child_window = NULL;
    g_pending_child_toggle = FALSE;
    g_pending_child_output = FALSE;
    if (g_child_process) {
        if (!had_window &&
            WaitForSingleObject(g_child_process, 0) == WAIT_TIMEOUT) {
            /* The process never created a window; do not leave an orphan. */
            TerminateProcess(g_child_process, 0);
        }
        CloseHandle(g_child_process);
        g_child_process = NULL;
    }
}

static void initialize_standalone_preferences(void) {
    PWSTR local = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT, NULL, &local)) && local) {
        swprintf(g_app_data_root, ARRAY_LEN(g_app_data_root),
                 L"%ls\\Charter Media Player", local);
        wchar_t legacy[MAX_PATH * 4];
        swprintf(legacy, ARRAY_LEN(legacy),
                 L"%ls\\Charter Music Browser", local);
        if (directory_exists(legacy) &&
            (!directory_exists(g_app_data_root) ||
             !directory_tree_has_files(g_app_data_root)))
            wcscpy(g_app_data_root, legacy);
        CoTaskMemFree(local);
        swprintf(g_player_config_path, ARRAY_LEN(g_player_config_path),
                 L"%ls\\Player Settings.cfg", g_app_data_root);
        swprintf(g_tools_root, ARRAY_LEN(g_tools_root),
                 L"%ls\\Tools", g_app_data_root);
        load_player_preferences();
        refresh_tool_paths();
    }
    enumerate_audio_outputs();
    /* A video opened directly from Windows should start on the normal output.
       Library-launched videos still follow Charter's selected audio device. */
    if (g_standalone_role == STANDALONE_VIDEO && !g_standalone_parent)
        g_audio_output_index = 0;
    if (g_volume) slider_set_value(g_volume, g_volume_percent);
    apply_player_volume();
}

static void create_standalone_ui(HWND hwnd) {
    create_fonts();
    if (g_standalone_role == STANDALONE_AUDIO) {
        g_play = make_button(hwnd, ID_PLAY, L"");
        g_seek = CreateWindowExW(0, SLIDER_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, S(300), S(24),
            hwnd, (HMENU)(INT_PTR)ID_SEEK, g_instance, NULL);
        g_volume = CreateWindowExW(0, SLIDER_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, S(100), S(24),
            hwnd, (HMENU)(INT_PTR)ID_VOLUME, g_instance, NULL);
    } else {
        g_video_window = CreateWindowExW(0, VIDEO_CLASS,
            L"Charter Media Player - Video",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, S(960), S(600),
            hwnd, NULL, g_instance, NULL);
    }
}
