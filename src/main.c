/*
 * Charter Media Player unity entry point.
 *
 * Feature implementations are split into focused files while remaining in one
 * translation unit so private Win32 application state stays private and type-safe.
 */
#include "charter_internal.h"

#include "core.inc.c"
#include "player_layout.inc.c"
#include "themes.inc.c"
#include "windows_integration.inc.c"
#include "downloads.inc.c"
#include "matching.inc.c"
#include "library.inc.c"
#include "video.inc.c"
#include "discord_art.inc.c"
#include "playback.inc.c"
#include "app_lifecycle.inc.c"
#include "ui_controls.inc.c"
#include "ui_shell.inc.c"

static HANDLE g_single_instance_mutex;
static BOOL g_single_instance_mutex_owned;

static void release_single_instance_startup_lock(void) {
    if (g_single_instance_mutex && g_single_instance_mutex_owned) {
        ReleaseMutex(g_single_instance_mutex);
        g_single_instance_mutex_owned = FALSE;
    }
}

static void close_single_instance_mutex(void) {
    release_single_instance_startup_lock();
    if (g_single_instance_mutex) {
        CloseHandle(g_single_instance_mutex);
        g_single_instance_mutex = NULL;
    }
}

static void activate_existing_instance(HWND existing, const wchar_t *media_path) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(existing, &process_id);
    if (process_id) AllowSetForegroundWindow(process_id);

    if (media_path && media_path[0]) {
        COPYDATASTRUCT data;
        data.dwData = APP_COPYDATA_OPEN_MEDIA;
        data.cbData = (DWORD)((wcslen(media_path) + 1) * sizeof(wchar_t));
        data.lpData = (PVOID)media_path;
        DWORD_PTR handled = 0;
        SendMessageTimeoutW(existing, WM_COPYDATA, 0, (LPARAM)&data,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 3000, &handled);
    }
    /* Older builds can still restore their hidden window via this tray message. */
    PostMessageW(existing, WM_APP_BACKGROUND_TRAY, 0, WM_LBUTTONUP);
    ShowWindow(existing, SW_RESTORE);
    SetForegroundWindow(existing);
}

static BOOL claim_single_instance(const wchar_t *media_path) {
    g_single_instance_mutex = CreateMutexW(
        NULL, TRUE, L"Local\\SuperNetwork.CharterMediaPlayer.SingleInstance");
    if (g_single_instance_mutex) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            DWORD result = WaitForSingleObject(g_single_instance_mutex, 15000);
            if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED)
                g_single_instance_mutex_owned = TRUE;
            else {
                HWND existing = FindWindowW(APP_CLASS, NULL);
                if (existing) activate_existing_instance(existing, media_path);
                else MessageBoxW(NULL,
                    L"Charter Media Player is already starting. Please try again in a moment.",
                    APP_TITLE, MB_OK | MB_ICONINFORMATION);
                close_single_instance_mutex();
                return FALSE;
            }
        } else {
            g_single_instance_mutex_owned = TRUE;
        }
    }

    HWND existing = FindWindowW(APP_CLASS, NULL);
    if (existing) {
        activate_existing_instance(existing, media_path);
        close_single_instance_mutex();
        return FALSE;
    }
    return TRUE;
}

static BOOL register_classes(void) {
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = g_app_icon ? g_app_icon : LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = g_app_icon_small ? g_app_icon_small : wc.hIcon;
    wc.hbrBackground = NULL;
    wc.lpszClassName = APP_CLASS;
    wc.style = 0;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = button_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = BUTTON_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = search_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = NULL;
    wc.lpszClassName = SEARCH_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = input_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = NULL;
    wc.lpszClassName = INPUT_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = slider_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = SLIDER_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = playlist_dialog_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = PLAYLIST_DIALOG_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = video_window_proc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = g_app_icon ? g_app_icon : LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = g_app_icon_small ? g_app_icon_small : wc.hIcon;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = VIDEO_CLASS;
    if (!RegisterClassExW(&wc)) return FALSE;

    return TRUE;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, PWSTR cmd_line, int show) {
    (void)prev;
    (void)cmd_line;
    g_instance = instance;
    srand((unsigned)GetTickCount());

    int argument_count = 0;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments && argument_count > 1) {
        wcsncpy(g_micro_media_path, arguments[1],
                ARRAY_LEN(g_micro_media_path) - 1);
        g_micro_media_path[ARRAY_LEN(g_micro_media_path) - 1] = L'\0';
    }
    if (arguments) LocalFree(arguments);

    if (!claim_single_instance(g_micro_media_path)) return 0;

    initialize_theme_palette();

    SetProcessDPIAware();
    enable_dark_app_mode();

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    HRESULT ole_hr = OleInitialize(NULL);
    (void)ole_hr;
    InitializeCriticalSection(&g_audio_lock);
    HRESULT mf_hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (FAILED(mf_hr)) {
        DeleteCriticalSection(&g_audio_lock);
        OleUninitialize();
        close_single_instance_mutex();
        MessageBoxW(NULL, L"Windows Media Foundation could not be started.", APP_TITLE, MB_ICONERROR);
        return 1;
    }

    g_search_brush = CreateSolidBrush(C_SURFACE);
    g_sidebar_brush = CreateSolidBrush(C_SIDEBAR);
    g_app_icon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    g_app_icon_small = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    /* Keep a high-resolution source icon and let DrawIconEx downsample at the
       current DPI. Loading at the old on-screen size forced Windows to upscale
       a tiny raster whenever the monitor scale changed. */
#define LOAD_UI_ICON(resource_id) \
    (HICON)LoadImageW(instance, MAKEINTRESOURCEW(resource_id), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR)
    g_icon_home = LOAD_UI_ICON(IDI_HOME_ICON);
    g_icon_downloads = LOAD_UI_ICON(IDI_DOWNLOADS_ICON);
    g_icon_settings = LOAD_UI_ICON(IDI_SETTINGS_ICON);
    g_icon_folder = LOAD_UI_ICON(IDI_FOLDER_ICON);
    g_icon_add = LOAD_UI_ICON(IDI_ADD_ICON);
    g_icon_trash = LOAD_UI_ICON(IDI_TRASH_ICON);
    g_icon_pencil = LOAD_UI_ICON(IDI_PENCIL_ICON);
    g_icon_search = LOAD_UI_ICON(IDI_SEARCH_ICON);
    g_icon_close = LOAD_UI_ICON(IDI_CLOSE_ICON);
    g_icon_refresh = LOAD_UI_ICON(IDI_REFRESH_ICON);
    g_icon_loop = LOAD_UI_ICON(IDI_LOOP_ICON);
    g_icon_shuffle = LOAD_UI_ICON(IDI_SHUFFLE_ICON);
    g_icon_smart = LOAD_UI_ICON(IDI_SMART_ICON);
    g_icon_blacklist = LOAD_UI_ICON(IDI_BLACKLIST_ICON);
    g_icon_star = LOAD_UI_ICON(IDI_STAR_ICON);
    g_icon_heart = LOAD_UI_ICON(IDI_HEART_ICON);
    g_icon_musical = LOAD_UI_ICON(IDI_MUSICAL_ICON);
    g_icon_youtube = LOAD_UI_ICON(IDI_YOUTUBE_ICON);
#undef LOAD_UI_ICON

    if (!register_classes()) {
        MessageBoxW(NULL, L"Could not register the application window.", APP_TITLE, MB_ICONERROR);
        if (g_search_brush) DeleteObject(g_search_brush);
        if (g_sidebar_brush) DeleteObject(g_sidebar_brush);
        if (g_app_icon) DestroyIcon(g_app_icon);
        if (g_app_icon_small) DestroyIcon(g_app_icon_small);
        MFShutdown();
        DeleteCriticalSection(&g_audio_lock);
        OleUninitialize();
        close_single_instance_mutex();
        return 1;
    }

    g_main = CreateWindowExW(0, APP_CLASS, APP_TITLE,
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, S(1240), S(780),
                             NULL, NULL, instance, NULL);
    if (!g_main) {
        MessageBoxW(NULL, L"Could not create the application window.", APP_TITLE, MB_ICONERROR);
        if (g_search_brush) DeleteObject(g_search_brush);
        if (g_sidebar_brush) DeleteObject(g_sidebar_brush);
        if (g_app_icon) DestroyIcon(g_app_icon);
        if (g_app_icon_small) DestroyIcon(g_app_icon_small);
        MFShutdown();
        DeleteCriticalSection(&g_audio_lock);
        OleUninitialize();
        close_single_instance_mutex();
        return 1;
    }

    release_single_instance_startup_lock();

    BOOL opened_external = FALSE;
    if (g_micro_media_path[0]) {
        g_external_launch = TRUE;
        opened_external = play_external_media_at(g_micro_media_path, 0, FALSE);
        if (!opened_external) {
            g_external_launch = FALSE;
            set_status(is_media_extension(g_micro_media_path)
                ? L"Charter could not open the requested media file."
                : L"That file extension is not supported by Charter Media Player.");
            if (g_library_ready) refresh_library();
        }
    }

    if (opened_external && is_video_extension(g_micro_media_path)) {
        ShowWindow(g_main, SW_HIDE);
    } else {
        if (opened_external) enter_micro_player_mode();
        ShowWindow(g_main, show);
        UpdateWindow(g_main);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        /* Keep the video-window shortcuts working when one of its child
           controls (for example the seek bar) owns keyboard focus. */
        if (g_video_window && IsWindowVisible(g_video_window) &&
            (msg.hwnd == g_video_window || IsChild(g_video_window, msg.hwnd)) &&
            ((msg.message == WM_KEYDOWN &&
              (msg.wParam == VK_F11 || msg.wParam == VK_ESCAPE)) ||
             (msg.message == WM_SYSKEYDOWN && msg.wParam == VK_RETURN))) {
            SendMessageW(g_video_window, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    wait_for_library_workers();

    destroy_fonts();
    if (g_search_brush) DeleteObject(g_search_brush);
    if (g_sidebar_brush) DeleteObject(g_sidebar_brush);
    if (g_app_icon) DestroyIcon(g_app_icon);
    if (g_app_icon_small) DestroyIcon(g_app_icon_small);
    if (g_icon_home) DestroyIcon(g_icon_home);
    if (g_icon_downloads) DestroyIcon(g_icon_downloads);
    if (g_icon_settings) DestroyIcon(g_icon_settings);
    if (g_icon_folder) DestroyIcon(g_icon_folder);
    if (g_icon_add) DestroyIcon(g_icon_add);
    if (g_icon_trash) DestroyIcon(g_icon_trash);
    if (g_icon_pencil) DestroyIcon(g_icon_pencil);
    if (g_icon_search) DestroyIcon(g_icon_search);
    if (g_icon_close) DestroyIcon(g_icon_close);
    if (g_icon_refresh) DestroyIcon(g_icon_refresh);
    if (g_icon_loop) DestroyIcon(g_icon_loop);
    if (g_icon_shuffle) DestroyIcon(g_icon_shuffle);
    if (g_icon_smart) DestroyIcon(g_icon_smart);
    if (g_icon_blacklist) DestroyIcon(g_icon_blacklist);
    if (g_icon_star) DestroyIcon(g_icon_star);
    if (g_icon_heart) DestroyIcon(g_icon_heart);
    if (g_icon_musical) DestroyIcon(g_icon_musical);
    if (g_icon_youtube) DestroyIcon(g_icon_youtube);
    MFShutdown();
    DeleteCriticalSection(&g_audio_lock);
    OleUninitialize();
    close_single_instance_mutex();

    if (g_storage_migration_requested) {
        BOOL migrated = perform_requested_storage_migration();
        if (migrated) {
            MessageBoxW(NULL,
                        g_migration_status,
                        L"Charter Media Player migration",
                        MB_OK | MB_ICONINFORMATION);
            wchar_t executable[MAX_PATH * 4];
            DWORD length = GetModuleFileNameW(NULL, executable, ARRAY_LEN(executable));
            if (length > 0 && length < ARRAY_LEN(executable))
                ShellExecuteW(NULL, L"open", executable, NULL, NULL, SW_SHOWNORMAL);
        } else {
            MessageBoxW(NULL,
                        L"The legacy folder could not be moved. No legacy files were deleted. "
                        L"Close programs using that folder and try the migrator again.",
                        L"Charter Media Player migration",
                        MB_OK | MB_ICONERROR);
        }
    }
    return (int)msg.wParam;
}
