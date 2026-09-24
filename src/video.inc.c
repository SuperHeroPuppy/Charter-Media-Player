/* In-app video window and MFPlay callback. */

typedef struct VideoPlayerCallback {
    IMFPMediaPlayerCallback iface;
    LONG refs;
} VideoPlayerCallback;

static const GUID IID_IMFPMediaPlayerCallback_Local =
    { 0x766c8ffb, 0x5fdb, 0x4fea, { 0xa2, 0x8d, 0xb9, 0x12, 0x99, 0x6f, 0x51, 0xbd } };

static HRESULT STDMETHODCALLTYPE video_callback_query(IMFPMediaPlayerCallback *iface,
                                                       REFIID riid, void **object) {
    if (!object) return E_POINTER;
    *object = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMFPMediaPlayerCallback_Local)) {
        *object = iface;
        IMFPMediaPlayerCallback_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE video_callback_add_ref(IMFPMediaPlayerCallback *iface) {
    VideoPlayerCallback *cb = (VideoPlayerCallback *)iface;
    return (ULONG)InterlockedIncrement(&cb->refs);
}

static ULONG STDMETHODCALLTYPE video_callback_release(IMFPMediaPlayerCallback *iface) {
    VideoPlayerCallback *cb = (VideoPlayerCallback *)iface;
    LONG refs = InterlockedDecrement(&cb->refs);
    if (refs < 1) {
        cb->refs = 1; /* Static callback object; MFPlay does not own its storage. */
        return 1;
    }
    return (ULONG)refs;
}

static void STDMETHODCALLTYPE video_callback_event(IMFPMediaPlayerCallback *iface,
                                                    MFP_EVENT_HEADER *event) {
    (void)iface;
    if (!event || !g_main) return;
    if (g_video_player && event->pMediaPlayer != g_video_player) return;
    if (!g_video_player && event->eEventType != MFP_EVENT_TYPE_MEDIAITEM_SET) return;
    if (FAILED(event->hrEvent) || event->eEventType == MFP_EVENT_TYPE_ERROR) {
        PostMessageW(g_main, WM_APP_VIDEO_COMPLETE, 1, 0);
    } else if (event->eEventType == MFP_EVENT_TYPE_MEDIAITEM_SET) {
        PostMessageW(g_main, WM_APP_VIDEO_MEDIA_READY, 0,
                     (LPARAM)event->pMediaPlayer);
    } else if (event->eEventType == MFP_EVENT_TYPE_PLAYBACK_ENDED) {
        PostMessageW(g_main, WM_APP_VIDEO_COMPLETE, 0, 0);
    }
}

static IMFPMediaPlayerCallbackVtbl g_video_callback_vtbl = {
    video_callback_query,
    video_callback_add_ref,
    video_callback_release,
    video_callback_event
};

static VideoPlayerCallback g_video_callback = {
    { &g_video_callback_vtbl }, 1
};

static void update_video_play_label(void) {
    if (g_video_play) SetWindowTextW(g_video_play, g_is_paused ? L"Play" : L"Pause");
    if (g_video_play) InvalidateRect(g_video_play, NULL, FALSE);
}

static void layout_video_window(HWND hwnd) {
    if (!hwnd) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    int bar_h = g_video_controls_visible ? S(72) : 0;
    int bar_top = max(0, rc.bottom - bar_h);
    MoveWindow(g_video_surface, 0, 0, rc.right, bar_top, TRUE);
    if (g_video_controls_visible) {
        MoveWindow(g_video_play, S(18), bar_top + S(18), S(78), S(38), TRUE);
        MoveWindow(g_video_seek, S(112), bar_top + S(25),
                   max(S(180), rc.right - S(268)), S(24), TRUE);
        MoveWindow(g_video_fullscreen_button, max(S(18), rc.right - S(140)),
                   bar_top + S(18), S(122), S(38), TRUE);
    }
    if (g_video_player) IMFPMediaPlayer_UpdateVideo(g_video_player);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void show_video_controls(BOOL show) {
    if (!g_video_window) return;
    if (!show && (g_is_paused || g_seek_dragging)) return;
    g_video_controls_visible = show;
    g_video_last_activity = GetTickCount64();
    ShowWindow(g_video_play, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_video_seek, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_video_fullscreen_button, show ? SW_SHOW : SW_HIDE);
    if (!show && (GetFocus() == g_video_play || GetFocus() == g_video_seek ||
                  GetFocus() == g_video_fullscreen_button))
        SetFocus(g_video_window);
    layout_video_window(g_video_window);
}

static void set_video_fullscreen(BOOL fullscreen) {
    if (!g_video_window || fullscreen == g_video_fullscreen) return;
    if (fullscreen) {
        ZeroMemory(&g_video_windowed_placement, sizeof(g_video_windowed_placement));
        g_video_windowed_placement.length = sizeof(g_video_windowed_placement);
        GetWindowPlacement(g_video_window, &g_video_windowed_placement);
        g_video_windowed_style = GetWindowLongPtrW(g_video_window, GWL_STYLE);
        g_video_windowed_ex_style = GetWindowLongPtrW(g_video_window, GWL_EXSTYLE);
        HMONITOR monitor = MonitorFromWindow(g_video_window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) return;
        SetWindowLongPtrW(g_video_window, GWL_STYLE,
                          (g_video_windowed_style & ~WS_OVERLAPPEDWINDOW) |
                          WS_POPUP | WS_CLIPCHILDREN);
        SetWindowLongPtrW(g_video_window, GWL_EXSTYLE,
                          g_video_windowed_ex_style & ~WS_EX_WINDOWEDGE);
        SetWindowPos(g_video_window, HWND_TOP,
                     info.rcMonitor.left, info.rcMonitor.top,
                     info.rcMonitor.right - info.rcMonitor.left,
                     info.rcMonitor.bottom - info.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_video_fullscreen = TRUE;
    } else {
        SetWindowLongPtrW(g_video_window, GWL_STYLE, g_video_windowed_style);
        SetWindowLongPtrW(g_video_window, GWL_EXSTYLE, g_video_windowed_ex_style);
        SetWindowPlacement(g_video_window, &g_video_windowed_placement);
        SetWindowPos(g_video_window, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        g_video_fullscreen = FALSE;
    }
    if (g_video_fullscreen_button) {
        SetWindowTextW(g_video_fullscreen_button,
                       g_video_fullscreen ? L"Windowed" : L"Full screen");
        InvalidateRect(g_video_fullscreen_button, NULL, FALSE);
    }
    show_video_controls(TRUE);
}

static LRESULT CALLBACK video_surface_subclass_proc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
    UINT_PTR subclass_id, DWORD_PTR reference) {
    (void)subclass_id;
    (void)reference;
    switch (msg) {
        case WM_MOUSEMOVE:
            PostMessageW(g_video_window, WM_APP_VIDEO_ACTIVITY, 0, 0);
            break;
        case WM_LBUTTONDOWN:
            SetFocus(g_video_window);
            PostMessageW(g_video_window, WM_APP_VIDEO_ACTIVITY, 0, 0);
            break;
        case WM_LBUTTONDBLCLK:
            set_video_fullscreen(!g_video_fullscreen);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            return SendMessageW(g_video_window, msg, wParam, lParam);
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, video_surface_subclass_proc, 1);
            break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK video_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
            int corners = 2;
            DwmSetWindowAttribute(hwnd, 33, &corners, sizeof(corners));
            g_video_surface = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_BLACKRECT | SS_NOTIFY,
                0, 0, 100, 100, hwnd, NULL, g_instance, NULL);
            SetWindowSubclass(g_video_surface, video_surface_subclass_proc, 1, 0);
            g_video_play = CreateWindowExW(0, BUTTON_CLASS, L"Pause",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, S(78), S(36), hwnd, (HMENU)(INT_PTR)ID_VIDEO_PLAY, g_instance, NULL);
            g_video_seek = CreateWindowExW(0, SLIDER_CLASS, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, S(400), S(24), hwnd, (HMENU)(INT_PTR)ID_VIDEO_SEEK, g_instance, NULL);
            slider_set_value(g_video_seek, 0);
            g_video_fullscreen_button = CreateWindowExW(
                0, BUTTON_CLASS, L"Full screen",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, S(122), S(36), hwnd,
                (HMENU)(INT_PTR)ID_VIDEO_FULLSCREEN, g_instance, NULL);
            g_video_controls_visible = TRUE;
            g_video_last_activity = GetTickCount64();
            SetTimer(hwnd, 2, 250, NULL);
            return 0;
        }
        case WM_SIZE:
            layout_video_window(hwnd);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_VIDEO_PLAY && HIWORD(wParam) == BN_CLICKED) {
                pause_resume();
                return 0;
            }
            if (LOWORD(wParam) == ID_VIDEO_FULLSCREEN && HIWORD(wParam) == BN_CLICKED) {
                set_video_fullscreen(!g_video_fullscreen);
                return 0;
            }
            break;
        case WM_APP_VIDEO_ACTIVITY:
            show_video_controls(TRUE);
            return 0;
        case WM_TIMER:
            if (wParam == 2 && g_video_controls_visible && g_is_playing &&
                !g_is_paused && !g_seek_dragging &&
                GetTickCount64() - g_video_last_activity >= 2200) {
                POINT pointer;
                GetCursorPos(&pointer);
                ScreenToClient(hwnd, &pointer);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (pointer.y < rc.bottom - S(72)) show_video_controls(FALSE);
            }
            return 0;
        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(C_PLAYER);
            FillRect((HDC)wParam, &rc, brush);
            DeleteObject(brush);
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            if (g_video_controls_visible) {
                RECT bar = { 0, max(0, rc.bottom - S(72)), rc.right, rc.bottom };
                HBRUSH brush = CreateSolidBrush(C_PLAYER);
                FillRect(dc, &bar, brush);
                DeleteObject(brush);
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, C_TEXT_DIM);
                SelectObject(dc, g_font_small);
                RECT hint = { S(112), bar.top + S(2), rc.right - S(150), bar.top + S(22) };
                DrawTextW(dc, L"Space: play/pause  |  F11 or double-click: full screen  |  Esc: close", -1, &hint,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_SPACE) { pause_resume(); return 0; }
            if (wParam == VK_F11) { set_video_fullscreen(!g_video_fullscreen); return 0; }
            if (wParam == VK_ESCAPE) {
                if (g_video_fullscreen) set_video_fullscreen(FALSE);
                else stop_playback();
                return 0;
            }
            break;
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN && (lParam & (1u << 29))) {
                set_video_fullscreen(!g_video_fullscreen);
                return 0;
            }
            break;
        case WM_CLOSE:
            stop_playback();
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            mmi->ptMinTrackSize.x = S(560);
            mmi->ptMinTrackSize.y = S(360);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 2);
            g_video_surface = NULL;
            g_video_play = NULL;
            g_video_seek = NULL;
            g_video_fullscreen_button = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
