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
    if (!event || !g_main || !g_video_player || event->pMediaPlayer != g_video_player) return;
    if (FAILED(event->hrEvent) || event->eEventType == MFP_EVENT_TYPE_ERROR) {
        PostMessageW(g_main, WM_APP_VIDEO_COMPLETE, 1, 0);
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

static LRESULT CALLBACK video_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
            int corners = 2;
            DwmSetWindowAttribute(hwnd, 33, &corners, sizeof(corners));
            g_video_surface = CreateWindowExW(0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                0, 0, 100, 100, hwnd, NULL, g_instance, NULL);
            g_video_play = CreateWindowExW(0, BUTTON_CLASS, L"Pause",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, S(78), S(36), hwnd, (HMENU)(INT_PTR)ID_VIDEO_PLAY, g_instance, NULL);
            g_video_seek = CreateWindowExW(0, SLIDER_CLASS, L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, S(400), S(24), hwnd, (HMENU)(INT_PTR)ID_VIDEO_SEEK, g_instance, NULL);
            slider_set_value(g_video_seek, 0);
            return 0;
        }
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int bar_h = S(68);
            int bar_top = max(0, rc.bottom - bar_h);
            MoveWindow(g_video_surface, 0, 0, rc.right, bar_top, TRUE);
            MoveWindow(g_video_play, S(18), bar_top + S(16), S(78), S(38), TRUE);
            MoveWindow(g_video_seek, S(112), bar_top + S(23), max(S(180), rc.right - S(130)), S(24), TRUE);
            if (g_video_player) IMFPMediaPlayer_UpdateVideo(g_video_player);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_VIDEO_PLAY && HIWORD(wParam) == BN_CLICKED) {
                pause_resume();
                return 0;
            }
            break;
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
            RECT bar = { 0, max(0, rc.bottom - S(68)), rc.right, rc.bottom };
            HBRUSH brush = CreateSolidBrush(C_PLAYER);
            FillRect(dc, &bar, brush);
            DeleteObject(brush);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_TEXT_DIM);
            SelectObject(dc, g_font_small);
            RECT hint = { S(112), bar.top + S(2), rc.right - S(18), bar.top + S(22) };
            DrawTextW(dc, L"Seek  |  Space: play/pause  |  Esc: stop and close", -1, &hint,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_SPACE) { pause_resume(); return 0; }
            if (wParam == VK_ESCAPE) { stop_playback(); return 0; }
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
            g_video_surface = NULL;
            g_video_play = NULL;
            g_video_seek = NULL;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

