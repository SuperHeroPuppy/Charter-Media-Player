/* Theme primitives and reusable custom controls. */

static HFONT make_font(int logical_height, int weight, const wchar_t *face) {
    return CreateFontW(-S(logical_height), 0, 0, 0, weight,
                       FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void destroy_fonts(void) {
    if (g_font_body) DeleteObject(g_font_body);
    if (g_font_body_semibold) DeleteObject(g_font_body_semibold);
    if (g_font_small) DeleteObject(g_font_small);
    if (g_font_small_semibold) DeleteObject(g_font_small_semibold);
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_section) DeleteObject(g_font_section);
    g_font_body = NULL;
    g_font_body_semibold = NULL;
    g_font_small = NULL;
    g_font_small_semibold = NULL;
    g_font_title = NULL;
    g_font_section = NULL;
}

static void create_fonts(void) {
    destroy_fonts();
    g_font_body = make_font(15, FW_NORMAL, L"Segoe UI Variable Text");
    g_font_body_semibold = make_font(15, FW_SEMIBOLD, L"Segoe UI Variable Text");
    g_font_small = make_font(13, FW_NORMAL, L"Segoe UI Variable Text");
    g_font_small_semibold = make_font(13, FW_SEMIBOLD, L"Segoe UI Variable Text");
    g_font_title = make_font(22, FW_SEMIBOLD, L"Segoe UI Variable Display");
    g_font_section = make_font(19, FW_SEMIBOLD, L"Segoe UI Variable Display");

    if (g_search) SendMessageW(g_search, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_url_edit) SendMessageW(g_url_edit, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_list) SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_list_header) SendMessageW(g_list_header, WM_SETFONT, (WPARAM)g_font_small_semibold, TRUE);
}

static void fill_round_rect(HDC dc, RECT r, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

static void stroke_round_rect(HDC dc, RECT r, int radius, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static void draw_line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width) {
    HPEN pen = CreatePen(PS_SOLID, max(1, width), color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void draw_app_mark(HDC dc, int x, int y, int size) {
    if (g_app_icon) {
        DrawIconEx(dc, x, y, g_app_icon, size, size, 0, NULL, DI_NORMAL);
        return;
    }

    RECT r = { x, y, x + size, y + size };
    fill_round_rect(dc, r, S(8), C_ACCENT);

    HPEN pen = CreatePen(PS_SOLID, max(2, S(2)), C_ACCENT_TEXT);
    HBRUSH brush = CreateSolidBrush(C_ACCENT_TEXT);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    int stem_x = x + size * 60 / 100;
    int top_y = y + size * 25 / 100;
    int bottom_y = y + size * 66 / 100;
    MoveToEx(dc, stem_x, top_y, NULL);
    LineTo(dc, stem_x, bottom_y);
    int rr = max(S(3), size / 9);
    Ellipse(dc, stem_x - rr, bottom_y - rr, stem_x + rr, bottom_y + rr);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static void draw_icon(HDC dc, int id, RECT r, COLORREF color) {
    HICON asset = NULL;
    if (id == ID_NAV_LIBRARY) asset = g_icon_home;
    else if (id == ID_NAV_DOWNLOADS) asset = g_icon_downloads;
    else if (id == ID_NAV_SETTINGS) asset = g_icon_settings;
    else if (id == ID_OPEN_LIBRARY || id == ID_OPEN_FOLDER) asset = g_icon_folder;
    else if (id == ID_ADD_PLAYLIST || id == ID_NEW_PLAYLIST || id == ID_IMPORT_MEDIA) asset = g_icon_add;
    else if (id == ID_REMOVE_PLAYLIST || id == ID_DELETE_MEDIA) asset = g_icon_trash;
    else if (id == ID_EDIT_PLAYLIST) asset = g_icon_pencil;
    else if (id == ID_REFRESH) asset = g_icon_refresh;
    else if (id == ID_LOOP) asset = g_icon_loop;
    else if (id == ID_SHUFFLE) asset = g_icon_shuffle;
    else if (id == ID_FORMAT) asset = g_icon_musical;
    else if (id == ID_STAR_MEDIA) asset = g_icon_star;
    else if (id == ID_HEART_MEDIA) asset = g_icon_heart;
    if (asset) {
        int size = min(r.right - r.left, r.bottom - r.top) - S(2);
        if (size < S(16)) size = S(16);
        int x = (r.left + r.right - size) / 2;
        int y = (r.top + r.bottom - size) / 2;
        DrawIconEx(dc, x, y, asset, size, size, 0, NULL, DI_NORMAL);
        return;
    }

    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;
    int u = S(1);
    HPEN pen = CreatePen(PS_SOLID, max(1, S(1)), color);
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, brush);

    if (id == ID_PLAY || (id == ID_PAUSE && g_is_paused)) {
        if (g_is_playing && id == ID_PLAY) {
            Rectangle(dc, cx - S(6), cy - S(7), cx - S(2), cy + S(7));
            Rectangle(dc, cx + S(2), cy - S(7), cx + S(6), cy + S(7));
        } else {
            POINT pts[3] = {
                { cx - S(4), cy - S(7) },
                { cx - S(4), cy + S(7) },
                { cx + S(7), cy }
            };
            Polygon(dc, pts, 3);
        }
    } else if (id == ID_PREVIOUS || id == ID_NEXT) {
        int dir = id == ID_PREVIOUS ? -1 : 1;
        int barx = cx + dir * S(7);
        draw_line(dc, barx, cy - S(7), barx, cy + S(7), color, max(1, S(2)));
        POINT pts[3] = {
            { cx + dir * S(5), cy },
            { cx - dir * S(5), cy - S(7) },
            { cx - dir * S(5), cy + S(7) }
        };
        Polygon(dc, pts, 3);
    } else if (id == ID_PAUSE) {
        Rectangle(dc, cx - S(6), cy - S(7), cx - S(2), cy + S(7));
        Rectangle(dc, cx + S(2), cy - S(7), cx + S(6), cy + S(7));
    } else if (id == ID_STOP) {
        Rectangle(dc, cx - S(6), cy - S(6), cx + S(6), cy + S(6));
    } else if (id == ID_REFRESH) {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Arc(dc, cx - S(8), cy - S(8), cx + S(8), cy + S(8),
            cx + S(7), cy - S(2), cx - S(5), cy - S(6));
        POINT arrow[3] = {
            { cx - S(7), cy - S(7) },
            { cx - S(1), cy - S(7) },
            { cx - S(5), cy - S(2) }
        };
        SelectObject(dc, brush);
        Polygon(dc, arrow, 3);
    } else if (id == ID_OPEN_LIBRARY || id == ID_OPEN_FOLDER) {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        RECT fr = { cx - S(8), cy - S(5), cx + S(8), cy + S(7) };
        RoundRect(dc, fr.left, fr.top, fr.right, fr.bottom, S(3), S(3));
        MoveToEx(dc, cx - S(7), cy - S(5), NULL);
        LineTo(dc, cx - S(2), cy - S(5));
        LineTo(dc, cx, cy - S(8));
        LineTo(dc, cx + S(4), cy - S(8));
        LineTo(dc, cx + S(6), cy - S(5));
        SelectObject(dc, brush);
        if (id == ID_OPEN_FOLDER) {
            draw_line(dc, cx + S(2), cy + S(1), cx + S(7), cy - S(4), color, max(1, u));
            draw_line(dc, cx + S(7), cy - S(4), cx + S(7), cy, color, max(1, u));
        }
    } else if (id == ID_DOWNLOAD) {
        if (InterlockedCompareExchange(&g_downloading, 0, 0)) {
            draw_line(dc, cx - S(6), cy - S(6), cx + S(6), cy + S(6), color, max(1, S(2)));
            draw_line(dc, cx + S(6), cy - S(6), cx - S(6), cy + S(6), color, max(1, S(2)));
        } else {
            draw_line(dc, cx, cy - S(8), cx, cy + S(4), color, max(1, S(2)));
            draw_line(dc, cx - S(5), cy, cx, cy + S(5), color, max(1, S(2)));
            draw_line(dc, cx, cy + S(5), cx + S(5), cy, color, max(1, S(2)));
            draw_line(dc, cx - S(8), cy + S(8), cx + S(8), cy + S(8), color, max(1, S(2)));
        }
    } else if (id == ID_INSTALL_TOOLS) {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - S(7), cy - S(7), cx + S(7), cy + S(7));
        Ellipse(dc, cx - S(2), cy - S(2), cx + S(2), cy + S(2));
        for (int a = 0; a < 4; ++a) {
            if (a == 0) draw_line(dc, cx, cy - S(10), cx, cy - S(6), color, max(1, S(2)));
            if (a == 1) draw_line(dc, cx + S(6), cy, cx + S(10), cy, color, max(1, S(2)));
            if (a == 2) draw_line(dc, cx, cy + S(6), cx, cy + S(10), color, max(1, S(2)));
            if (a == 3) draw_line(dc, cx - S(10), cy, cx - S(6), cy, color, max(1, S(2)));
        }
    } else if (id == ID_FORMAT) {
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        draw_line(dc, cx - S(6), cy - S(6), cx + S(2), cy - S(8), color, max(1, S(1)));
        draw_line(dc, cx + S(2), cy - S(8), cx + S(2), cy + S(3), color, max(1, S(2)));
        SelectObject(dc, brush);
        Ellipse(dc, cx - S(3), cy + S(1), cx + S(4), cy + S(8));
    } else if (id == ID_ADD_PLAYLIST || id == ID_NEW_PLAYLIST || id == ID_IMPORT_MEDIA) {
        draw_line(dc, cx - S(7), cy, cx + S(7), cy, color, max(1, S(2)));
        draw_line(dc, cx, cy - S(7), cx, cy + S(7), color, max(1, S(2)));
    } else if (id == ID_REMOVE_PLAYLIST) {
        draw_line(dc, cx - S(7), cy, cx + S(7), cy, color, max(1, S(2)));
    }

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

static BOOL button_is_primary(int id) {
    return id == ID_PLAY || id == ID_DOWNLOAD;
}

static BOOL button_is_nav(int id) {
    return id == ID_NAV_LIBRARY || id == ID_NAV_DOWNLOADS || id == ID_NAV_SETTINGS;
}

static BOOL button_has_icon(int id) {
    return id == ID_PLAY || id == ID_PREVIOUS || id == ID_NEXT || id == ID_PAUSE ||
           id == ID_STOP || id == ID_REFRESH || id == ID_OPEN_LIBRARY ||
           id == ID_OPEN_FOLDER || id == ID_DOWNLOAD || id == ID_INSTALL_TOOLS ||
           id == ID_FORMAT || id == ID_ADD_PLAYLIST || id == ID_NEW_PLAYLIST ||
           id == ID_REMOVE_PLAYLIST || id == ID_EDIT_PLAYLIST || id == ID_IMPORT_MEDIA ||
           id == ID_DELETE_MEDIA || id == ID_LOOP || id == ID_SHUFFLE ||
           id == ID_STAR_MEDIA || id == ID_HEART_MEDIA ||
           id == ID_NAV_LIBRARY || id == ID_NAV_DOWNLOADS || id == ID_NAV_SETTINGS;
}

static BOOL nav_is_selected(int id) {
    return (id == ID_NAV_LIBRARY && g_page == PAGE_LIBRARY) ||
           (id == ID_NAV_DOWNLOADS && g_page == PAGE_DOWNLOADS) ||
           (id == ID_NAV_SETTINGS && g_page == PAGE_SETTINGS);
}

static void paint_button(HWND hwnd, HDC dc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int id = GetDlgCtrlID(hwnd);
    LONG_PTR state = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    BOOL hover = (state & BTN_HOVER) != 0;
    BOOL pressed = (state & BTN_PRESSED) != 0;
    BOOL enabled = IsWindowEnabled(hwnd);
    BOOL primary = button_is_primary(id);

    COLORREF parent_fill = C_BG;
    if (button_is_nav(id) || id == ID_NEW_PLAYLIST) parent_fill = C_SIDEBAR;
    if (id == ID_PLAY || id == ID_PREVIOUS || id == ID_NEXT || id == ID_VIDEO_PLAY ||
        id == ID_LOOP || id == ID_SHUFFLE) parent_fill = C_PLAYER;
    if (id == ID_DOWNLOAD || id == ID_FORMAT || id == ID_INSTALL_TOOLS ||
        id == ID_DISCORD_TOGGLE || id == ID_DISCORD_SAVE || id == ID_DISCORD_MODE) parent_fill = C_CARD;
    HBRUSH outside = CreateSolidBrush(parent_fill);
    FillRect(dc, &rc, outside);
    DeleteObject(outside);

    COLORREF fill;
    COLORREF border;
    COLORREF text;

    if (primary) {
        fill = pressed ? C_ACCENT_PRESS : (hover ? C_ACCENT_HOVER : C_ACCENT);
        border = blend_color(fill, RGB(255, 255, 255), hover ? 8 : 3);
        text = C_ACCENT_TEXT;
    } else {
        fill = pressed ? C_SURFACE_PRESS : (hover ? C_SURFACE_HOVER : C_SURFACE);
        border = hover ? RGB(88, 88, 88) : RGB(66, 66, 66);
        text = C_TEXT;
    }

    if (button_is_nav(id)) {
        BOOL selected = nav_is_selected(id);
        fill = selected ? C_SURFACE : (hover ? C_SURFACE_HOVER : C_SIDEBAR);
        border = selected ? C_BORDER_SOFT : C_SIDEBAR;
        text = selected ? C_TEXT : C_TEXT_DIM;
    }

    Track *selected_media = (id == ID_STAR_MEDIA || id == ID_HEART_MEDIA) ? selected_track() : NULL;
    BOOL toggled = (id == ID_LOOP && g_loop_enabled) ||
                   (id == ID_SHUFFLE && g_shuffle_enabled) ||
                   (id == ID_DISCORD_TOGGLE && g_discord_enabled) ||
                   (id == ID_STAR_MEDIA && selected_media && selected_media->starred) ||
                   (id == ID_HEART_MEDIA && selected_media && selected_media->liked);
    if (toggled && enabled) {
        fill = blend_color(C_SURFACE, C_ACCENT, 20);
        border = C_ACCENT;
        text = C_ACCENT;
    }

    if (!enabled) {
        fill = C_CARD_ALT;
        border = RGB(57, 57, 57);
        text = C_TEXT_FAINT;
    }

    RECT face = rc;
    InflateRect(&face, -S(1), -S(1));
    fill_round_rect(dc, face, S(7), fill);
    stroke_round_rect(dc, face, S(7), border);

    wchar_t label[128];
    GetWindowTextW(hwnd, label, ARRAY_LEN(label));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, text);
    SelectObject(dc, g_font_body_semibold);

    RECT text_rc = face;
    BOOL has_icon = button_has_icon(id);
    int icon_area = has_icon ? S(28) : 0;
    int label_width = 0;
    if (label[0]) {
        SIZE ts = {0};
        GetTextExtentPoint32W(dc, label, (int)wcslen(label), &ts);
        label_width = ts.cx;
    }

    int spacing = (has_icon && label_width) ? S(6) : 0;
    int total = icon_area + spacing + label_width;
    int start_x = face.left + ((face.right - face.left) - total) / 2;
    if (button_is_nav(id)) start_x = face.left + S(14);
    if (has_icon) {
        RECT icon_rc = { start_x, face.top, start_x + icon_area, face.bottom };
        draw_icon(dc, id, icon_rc, text);
    }

    if (label_width) {
        text_rc.left = start_x + icon_area + spacing;
        text_rc.right = text_rc.left + label_width + S(2);
        DrawTextW(dc, label, -1, &text_rc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    if (button_is_nav(id) && nav_is_selected(id)) {
        RECT mark = { face.left + S(2), face.top + S(7), face.left + S(5), face.bottom - S(7) };
        fill_round_rect(dc, mark, S(2), C_ACCENT);
    }

    if (enabled && GetFocus() == hwnd && !button_is_nav(id)) {
        int y = face.bottom - S(3);
        draw_line(dc, face.left + S(10), y, face.right - S(10), y,
                  C_ACCENT, max(1, S(2)));
    }
}

static LRESULT CALLBACK button_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LONG_PTR state = GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_MOUSEMOVE:
            if (!(state & BTN_HOVER)) {
                state |= BTN_HOVER;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_MOUSELEAVE:
            state &= ~BTN_HOVER;
            if (GetCapture() != hwnd) state &= ~BTN_PRESSED;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONDOWN:
            if (IsWindowEnabled(hwnd)) {
                SetFocus(hwnd);
                SetCapture(hwnd);
                state |= BTN_PRESSED;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) {
                ReleaseCapture();
                state &= ~BTN_PRESSED;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT rc;
                GetClientRect(hwnd, &rc);
                InvalidateRect(hwnd, NULL, FALSE);
                if (PtInRect(&rc, pt)) {
                    SendMessageW(GetParent(hwnd), WM_COMMAND,
                                 MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
                }
            }
            return 0;

        case WM_CAPTURECHANGED:
            state &= ~BTN_PRESSED;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_KEYDOWN:
            if ((wParam == VK_SPACE || wParam == VK_RETURN) && IsWindowEnabled(hwnd)) {
                state |= BTN_PRESSED;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;

        case WM_KEYUP:
            if ((wParam == VK_SPACE || wParam == VK_RETURN) && (state & BTN_PRESSED)) {
                state &= ~BTN_PRESSED;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, state);
                InvalidateRect(hwnd, NULL, FALSE);
                SendMessageW(GetParent(hwnd), WM_COMMAND,
                             MAKEWPARAM(GetDlgCtrlID(hwnd), BN_CLICKED), (LPARAM)hwnd);
                return 0;
            }
            break;

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint_button(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK search_edit_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        HWND parent = GetParent(hwnd);
        if (parent) InvalidateRect(parent, NULL, FALSE);
    }
    return CallWindowProcW(g_old_search_edit_proc, hwnd, msg, wParam, lParam);
}

static void paint_search(HWND hwnd, HDC dc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    BOOL focused = (GetFocus() == g_search);

    HBRUSH outside = CreateSolidBrush(C_BG);
    FillRect(dc, &rc, outside);
    DeleteObject(outside);

    RECT face = rc;
    InflateRect(&face, -S(1), -S(1));
    fill_round_rect(dc, face, S(8), C_SURFACE);
    stroke_round_rect(dc, face, S(8), focused ? C_ACCENT : C_BORDER);

    if (g_icon_search) {
        DrawIconEx(dc, S(10), (rc.bottom - S(18)) / 2, g_icon_search,
                   S(18), S(18), 0, NULL, DI_NORMAL);
    } else {
        int cx = S(18);
        int cy = (rc.bottom - rc.top) / 2;
        int radius = S(5);
        HPEN pen = CreatePen(PS_SOLID, max(1, S(1)), C_TEXT_DIM);
        HGDIOBJ old_pen = SelectObject(dc, pen);
        HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, cx - radius, cy - radius - S(1), cx + radius, cy + radius - S(1));
        MoveToEx(dc, cx + S(4), cy + S(3), NULL);
        LineTo(dc, cx + S(8), cy + S(7));
        SelectObject(dc, old_brush);
        SelectObject(dc, old_pen);
        DeleteObject(pen);
    }
}

static LRESULT CALLBACK search_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (g_search && GetParent(g_search) == hwnd) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int edit_h = min(S(24), max(S(20), rc.bottom - S(12)));
                int edit_y = max(S(2), (rc.bottom - edit_h) / 2 - S(1));
                MoveWindow(g_search, S(38), edit_y, max(S(80), rc.right - S(50)), edit_h, TRUE);
            }
            return 0;

        case WM_COMMAND:
            SendMessageW(g_main, WM_COMMAND, wParam, lParam);
            return 0;

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, C_TEXT);
            SetBkColor(dc, C_SURFACE);
            return (LRESULT)g_search_brush;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint_search(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK url_edit_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) {
        HWND parent = GetParent(hwnd);
        if (parent) InvalidateRect(parent, NULL, FALSE);
    }
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(g_main, WM_COMMAND, MAKEWPARAM(ID_DOWNLOAD, BN_CLICKED), (LPARAM)g_download);
        return 0;
    }
    return CallWindowProcW(g_old_url_edit_proc, hwnd, msg, wParam, lParam);
}

static void paint_input(HWND hwnd, HDC dc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    BOOL focused = (GetFocus() == g_url_edit);

    HBRUSH outside = CreateSolidBrush(C_CARD);
    FillRect(dc, &rc, outside);
    DeleteObject(outside);

    RECT face = rc;
    InflateRect(&face, -S(1), -S(1));
    fill_round_rect(dc, face, S(8), C_SURFACE);
    stroke_round_rect(dc, face, S(8), focused ? C_ACCENT : C_BORDER);

    if (g_icon_youtube) {
        DrawIconEx(dc, S(11), (rc.bottom - S(22)) / 2, g_icon_youtube,
                   S(22), S(22), 0, NULL, DI_NORMAL);
        return;
    }

    int cx = S(20);
    int cy = (rc.bottom - rc.top) / 2;
    HPEN pen = CreatePen(PS_SOLID, max(1, S(1)), C_TEXT_DIM);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));

    Ellipse(dc, cx - S(7), cy - S(4), cx + S(1), cy + S(4));
    Ellipse(dc, cx - S(1), cy - S(4), cx + S(7), cy + S(4));
    draw_line(dc, cx - S(2), cy, cx + S(2), cy, C_TEXT_DIM, max(1, S(1)));

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

static LRESULT CALLBACK input_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            if (g_url_edit && GetParent(g_url_edit) == hwnd) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int edit_h = min(S(24), max(S(20), rc.bottom - S(12)));
                int edit_y = max(S(2), (rc.bottom - edit_h) / 2 - S(1));
                MoveWindow(g_url_edit, S(42), edit_y, max(S(80), rc.right - S(54)), edit_h, TRUE);
            }
            return 0;

        case WM_COMMAND:
            SendMessageW(g_main, WM_COMMAND, wParam, lParam);
            return 0;

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            SetTextColor(dc, C_TEXT);
            SetBkColor(dc, C_SURFACE);
            return (LRESULT)g_search_brush;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint_input(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int slider_max_for(HWND hwnd) {
    return GetDlgCtrlID(hwnd) == ID_VOLUME ? 100 : 1000;
}

static int slider_get_value(HWND hwnd) {
    if (!hwnd) return 0;
    return (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

static void slider_set_value(HWND hwnd, int value) {
    if (!hwnd) return;
    int max_value = slider_max_for(hwnd);
    if (value < 0) value = 0;
    if (value > max_value) value = max_value;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, value);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void slider_update_from_point(HWND hwnd, int x, BOOL notify) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int inset = S(8);
    int width = max(1, rc.right - rc.left - inset * 2);
    int px = x - inset;
    if (px < 0) px = 0;
    if (px > width) px = width;
    int value = MulDiv(px, slider_max_for(hwnd), width);
    slider_set_value(hwnd, value);
    if (notify) SendMessageW(g_main, WM_APP_SLIDER_CHANGED, (WPARAM)GetDlgCtrlID(hwnd), (LPARAM)value);
}

static LRESULT CALLBACK slider_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    switch (msg) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            int id = GetDlgCtrlID(hwnd);
            if (id == ID_SEEK || id == ID_VIDEO_SEEK) g_seek_dragging = TRUE;
            slider_update_from_point(hwnd, GET_X_LPARAM(lParam), id == ID_VOLUME);
            return 0;
        case WM_MOUSEMOVE:
            if (GetCapture() == hwnd && (wParam & MK_LBUTTON)) {
                slider_update_from_point(hwnd, GET_X_LPARAM(lParam), GetDlgCtrlID(hwnd) == ID_VOLUME);
            }
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) {
                slider_update_from_point(hwnd, GET_X_LPARAM(lParam), TRUE);
                ReleaseCapture();
                if (GetDlgCtrlID(hwnd) == ID_SEEK || GetDlgCtrlID(hwnd) == ID_VIDEO_SEEK) g_seek_dragging = FALSE;
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (GetDlgCtrlID(hwnd) == ID_SEEK || GetDlgCtrlID(hwnd) == ID_VIDEO_SEEK) g_seek_dragging = FALSE;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH bg = CreateSolidBrush(C_PLAYER);
            FillRect(dc, &rc, bg);
            DeleteObject(bg);

            int inset = S(8);
            int cy = (rc.bottom - rc.top) / 2;
            RECT track = { inset, cy - S(2), rc.right - inset, cy + S(2) };
            fill_round_rect(dc, track, S(2), RGB(78, 78, 78));

            int max_value = slider_max_for(hwnd);
            int value = slider_get_value(hwnd);
            int x = track.left + MulDiv(track.right - track.left, value, max_value);
            RECT fill = track;
            fill.right = x;
            if (fill.right > fill.left) fill_round_rect(dc, fill, S(2), C_ACCENT);

            HBRUSH thumb = CreateSolidBrush(C_TEXT);
            HGDIOBJ old_brush = SelectObject(dc, thumb);
            HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
            int r = S(5);
            Ellipse(dc, x - r, cy - r, x + r + 1, cy + r + 1);
            SelectObject(dc, old_pen);
            SelectObject(dc, old_brush);
            DeleteObject(thumb);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static LRESULT header_custom_draw(NMCUSTOMDRAW *cd) {
    if (cd->dwDrawStage == CDDS_PREPAINT) {
        /*
         * The native header paints the area to the right of the final column
         * itself. In dark mode that unused strip can fall back to the system
         * light brush, producing the bright white block seen in the list.
         * Ask for a post-paint pass so we can explicitly paint that remainder.
         */
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }

    if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
        HDC dc = cd->hdc;
        RECT rc = cd->rc;
        HBRUSH bg = CreateSolidBrush(C_HEADER);
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        int index = (int)cd->dwItemSpec;
        wchar_t text[128] = L"";
        HDITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = HDI_TEXT | HDI_FORMAT;
        item.pszText = text;
        item.cchTextMax = ARRAY_LEN(text);
        Header_GetItem(g_list_header, index, &item);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT_DIM);
        SelectObject(dc, g_font_small_semibold);
        RECT tr = rc;
        tr.left += S(12);
        tr.right -= S(8);
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        draw_line(dc, rc.right - 1, rc.top + S(8), rc.right - 1, rc.bottom - S(8), C_BORDER_SOFT, 1);
        draw_line(dc, rc.left, rc.bottom - 1, rc.right, rc.bottom - 1, C_BORDER_SOFT, 1);
        return CDRF_SKIPDEFAULT;
    }

    if (cd->dwDrawStage == CDDS_POSTPAINT) {
        RECT client;
        GetClientRect(g_list_header, &client);

        int fill_left = client.left;
        int count = Header_GetItemCount(g_list_header);
        if (count > 0) {
            RECT last;
            if (Header_GetItemRect(g_list_header, count - 1, &last)) {
                fill_left = last.right;
            }
        }

        if (fill_left < client.right) {
            RECT remainder = client;
            remainder.left = fill_left;
            HBRUSH bg = CreateSolidBrush(C_HEADER);
            FillRect(cd->hdc, &remainder, bg);
            DeleteObject(bg);
            draw_line(cd->hdc, remainder.left, remainder.bottom - 1,
                      remainder.right, remainder.bottom - 1, C_BORDER_SOFT, 1);
        }
        return CDRF_DODEFAULT;
    }

    return CDRF_DODEFAULT;
}

static LRESULT CALLBACK list_subclass_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                           UINT_PTR subclass_id, DWORD_PTR ref_data) {
    (void)subclass_id;
    (void)ref_data;

    if (msg == WM_NOTIFY) {
        NMHDR *hdr = (NMHDR *)lParam;
        if (hdr && hdr->hwndFrom == g_list_header && hdr->code == NM_CUSTOMDRAW) {
            return header_custom_draw((NMCUSTOMDRAW *)lParam);
        }
    }

    if (msg == WM_MOUSEMOVE) {
        LVHITTESTINFO hit = {0};
        hit.pt.x = GET_X_LPARAM(lParam);
        hit.pt.y = GET_Y_LPARAM(lParam);
        int target = ListView_HitTest(hwnd, &hit);
        if (g_list_dragging && GetCapture() == hwnd) {
            if (target >= 0 && target != g_list_drag_target) {
                g_list_drag_target = target;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        if (target != g_list_hover_row) {
            g_list_hover_row = target;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE) {
        if (g_list_hover_row != -1) {
            g_list_hover_row = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wParam == VK_DELETE) {
        if (g_page == PAGE_PLAYLIST) remove_selected_from_active_playlist();
        else if (g_page == PAGE_LIBRARY) delete_selected_media();
        update_button_enabled_state();
        return 0;
    }
    if (msg == WM_LBUTTONUP && g_list_dragging) {
        int source = g_list_drag_start;
        int target = g_list_drag_target;
        g_list_dragging = FALSE;
        g_list_drag_start = g_list_drag_target = -1;
        /* ReleaseCapture sends WM_CAPTURECHANGED synchronously. Clear our drag
           state first so that message cannot erase the saved source/target. */
        if (GetCapture() == hwnd) ReleaseCapture();
        reorder_media_rows(source, target);
        return 0;
    }
    if (msg == WM_CAPTURECHANGED && g_list_dragging) {
        g_list_dragging = FALSE;
        g_list_drag_start = g_list_drag_target = -1;
    }

    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, list_subclass_proc, 1);
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
