/*
 * Charter Media Player unity entry point.
 *
 * Feature implementations are split into focused files while remaining in one
 * translation unit so private Win32 application state stays private and type-safe.
 */
#include "charter_internal.h"

#include "core.inc.c"
#include "downloads.inc.c"
#include "library.inc.c"
#include "video.inc.c"
#include "playback.inc.c"
#include "app_lifecycle.inc.c"
#include "ui_controls.inc.c"
#include "ui_shell.inc.c"
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

    SetProcessDPIAware();

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
    g_icon_download = LOAD_UI_ICON(IDI_DOWNLOAD_ICON);
    g_icon_save = LOAD_UI_ICON(IDI_SAVE_ICON);
    g_icon_close = LOAD_UI_ICON(IDI_CLOSE_ICON);
    g_icon_refresh = LOAD_UI_ICON(IDI_REFRESH_ICON);
    g_icon_loop = LOAD_UI_ICON(IDI_LOOP_ICON);
    g_icon_shuffle = LOAD_UI_ICON(IDI_SHUFFLE_ICON);
    g_icon_star = LOAD_UI_ICON(IDI_STAR_ICON);
    g_icon_heart = LOAD_UI_ICON(IDI_HEART_ICON);
    g_icon_mute = LOAD_UI_ICON(IDI_MUTE_ICON);
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
        return 1;
    }

    ShowWindow(g_main, show);
    UpdateWindow(g_main);

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
    if (g_icon_download) DestroyIcon(g_icon_download);
    if (g_icon_save) DestroyIcon(g_icon_save);
    if (g_icon_close) DestroyIcon(g_icon_close);
    if (g_icon_refresh) DestroyIcon(g_icon_refresh);
    if (g_icon_loop) DestroyIcon(g_icon_loop);
    if (g_icon_shuffle) DestroyIcon(g_icon_shuffle);
    if (g_icon_star) DestroyIcon(g_icon_star);
    if (g_icon_heart) DestroyIcon(g_icon_heart);
    if (g_icon_mute) DestroyIcon(g_icon_mute);
    if (g_icon_musical) DestroyIcon(g_icon_musical);
    if (g_icon_youtube) DestroyIcon(g_icon_youtube);
    MFShutdown();
    DeleteCriticalSection(&g_audio_lock);
    OleUninitialize();

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
