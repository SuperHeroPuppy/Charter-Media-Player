/* Playlist dialog, main layout, painting, and window procedure. */

static void init_list_columns(void) {
    static const wchar_t *labels[] = { L"Name", L"Artist", L"File size" };
    for (int i = 0; i < 3; ++i) {
        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        col.pszText = (wchar_t *)labels[i];
        col.cx = S(120);
        col.iSubItem = i;
        col.fmt = (i == 2) ? LVCFMT_RIGHT : LVCFMT_LEFT;
        ListView_InsertColumn(g_list, i, &col);
    }
}

static HWND make_button(HWND parent, int id, const wchar_t *label) {
    return CreateWindowExW(0, BUTTON_CLASS, label,
                           WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                           0, 0, S(100), S(36), parent,
                           (HMENU)(INT_PTR)id, g_instance, NULL);
}

typedef struct PlaylistDialogState {
    BOOL accepted;
    BOOL editing;
    wchar_t name[128];
    wchar_t icon_source[MAX_PATH * 4];
    HWND name_edit;
    HWND icon_label;
} PlaylistDialogState;

#define ID_PL_NAME_EDIT   6101
#define ID_PL_ICON_PICK   6102
#define ID_PL_OK          6103
#define ID_PL_CANCEL      6104

static BOOL choose_playlist_icon_file(HWND owner, wchar_t *out, size_t out_count) {
    if (!out || out_count == 0) return FALSE;
    out[0] = L'\0';
    IFileOpenDialog *dialog = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IFileOpenDialog, (void **)&dialog);
    if (FAILED(hr) || !dialog) return FALSE;
    COMDLG_FILTERSPEC types[] = {
        { L"Images", L"*.png;*.jpg;*.jpeg;*.webp;*.bmp;*.gif;*.ico" },
        { L"All files", L"*.*" }
    };
    IFileOpenDialog_SetFileTypes(dialog, ARRAY_LEN(types), types);
    IFileOpenDialog_SetTitle(dialog, L"Choose playlist artwork");
    hr = IFileOpenDialog_Show(dialog, owner);
    if (SUCCEEDED(hr)) {
        IShellItem *item = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResult(dialog, &item)) && item) {
            PWSTR path = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path)) && path) {
                wcsncpy(out, path, out_count - 1);
                out[out_count - 1] = L'\0';
                CoTaskMemFree(path);
            }
            IShellItem_Release(item);
        }
    }
    IFileOpenDialog_Release(dialog);
    return out[0] != L'\0';
}

static LRESULT CALLBACK playlist_dialog_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PlaylistDialogState *st = (PlaylistDialogState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
            st = (PlaylistDialogState *)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
            BOOL dark = TRUE;
            DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

            HWND label = CreateWindowExW(0, L"STATIC", L"Playlist name",
                WS_CHILD | WS_VISIBLE, S(24), S(22), S(330), S(24), hwnd, NULL, g_instance, NULL);
            SendMessageW(label, WM_SETFONT, (WPARAM)g_font_body_semibold, TRUE);

            st->name_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->name,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                S(24), S(50), S(350), S(34), hwnd, (HMENU)(INT_PTR)ID_PL_NAME_EDIT, g_instance, NULL);
            SendMessageW(st->name_edit, WM_SETFONT, (WPARAM)g_font_body, TRUE);
            SetWindowTheme(st->name_edit, L"DarkMode_CFD", NULL);

            HWND artwork = CreateWindowExW(0, L"STATIC", L"Playlist artwork",
                WS_CHILD | WS_VISIBLE, S(24), S(102), S(330), S(24), hwnd, NULL, g_instance, NULL);
            SendMessageW(artwork, WM_SETFONT, (WPARAM)g_font_body_semibold, TRUE);

            make_button(hwnd, ID_PL_ICON_PICK, L"Choose image");
            st->icon_label = CreateWindowExW(0, L"STATIC",
                st->editing ? L"Keep current artwork, or choose a replacement" : L"Optional. Charter icon is used by default.",
                WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                S(24), S(166), S(350), S(24), hwnd, NULL, g_instance, NULL);
            SendMessageW(st->icon_label, WM_SETFONT, (WPARAM)g_font_small, TRUE);

            make_button(hwnd, ID_PL_OK, st->editing ? L"Save" : L"Create");
            make_button(hwnd, ID_PL_CANCEL, L"Cancel");

            HWND browse = GetDlgItem(hwnd, ID_PL_ICON_PICK);
            HWND ok = GetDlgItem(hwnd, ID_PL_OK);
            HWND cancel = GetDlgItem(hwnd, ID_PL_CANCEL);
            MoveWindow(browse, S(24), S(130), S(140), S(34), TRUE);
            MoveWindow(ok, S(194), S(208), S(84), S(36), TRUE);
            MoveWindow(cancel, S(290), S(208), S(84), S(36), TRUE);
            SetFocus(st->name_edit);
            SendMessageW(st->name_edit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (!st) return 0;
            if (id == ID_PL_ICON_PICK && HIWORD(wParam) == BN_CLICKED) {
                wchar_t path[MAX_PATH * 4];
                if (choose_playlist_icon_file(hwnd, path, ARRAY_LEN(path))) {
                    wcsncpy(st->icon_source, path, ARRAY_LEN(st->icon_source) - 1);
                    st->icon_source[ARRAY_LEN(st->icon_source) - 1] = L'\0';
                    SetWindowTextW(st->icon_label, base_name(path));
                }
                return 0;
            }
            if (id == ID_PL_OK && HIWORD(wParam) == BN_CLICKED) {
                GetWindowTextW(st->name_edit, st->name, ARRAY_LEN(st->name));
                while (st->name[0] && iswspace(st->name[wcslen(st->name) - 1]))
                    st->name[wcslen(st->name) - 1] = L'\0';
                wchar_t *first = st->name;
                while (*first && iswspace(*first)) ++first;
                if (first != st->name) memmove(st->name, first, (wcslen(first) + 1) * sizeof(wchar_t));
                if (!st->name[0]) {
                    MessageBoxW(hwnd, L"Enter a name for the playlist.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                st->accepted = TRUE;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == ID_PL_CANCEL && HIWORD(wParam) == BN_CLICKED) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)wParam;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_TEXT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH b = CreateSolidBrush(C_BG);
            FillRect(dc, &rc, b);
            DeleteObject(b);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static BOOL show_playlist_editor(int playlist_index) {
    PlaylistDialogState st;
    ZeroMemory(&st, sizeof(st));
    st.editing = playlist_index >= 0 && playlist_index < g_playlist_count;
    if (st.editing) {
        wcsncpy(st.name, g_playlists[playlist_index].name, ARRAY_LEN(st.name) - 1);
    } else {
        wcscpy(st.name, L"New playlist");
    }

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, PLAYLIST_DIALOG_CLASS,
        st.editing ? L"Edit playlist" : L"New playlist",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, S(420), S(300), g_main, NULL, g_instance, &st);
    if (!dlg) return FALSE;

    RECT dr, pr;
    GetWindowRect(dlg, &dr);
    GetWindowRect(g_main, &pr);
    SetWindowPos(dlg, HWND_TOP,
                 pr.left + ((pr.right - pr.left) - (dr.right - dr.left)) / 2,
                 pr.top + ((pr.bottom - pr.top) - (dr.bottom - dr.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    EnableWindow(g_main, FALSE);

    MSG msg;
    while (IsWindow(dlg) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(g_main, TRUE);
    SetForegroundWindow(g_main);

    if (!st.accepted) return FALSE;
    if (st.editing) {
        if (!update_playlist_details(playlist_index, st.name, st.icon_source)) return FALSE;
        g_active_playlist = min(playlist_index, g_playlist_count - 1);
        if (g_active_playlist >= 0) load_active_playlist();
    } else {
        int idx = create_new_playlist_named(st.name, st.icon_source);
        if (idx < 0) return FALSE;
        g_active_playlist = idx;
    }
    return TRUE;
}

static void update_page_visibility(void) {
    BOOL library_page = g_page == PAGE_LIBRARY;
    BOOL playlist_page = g_page == PAGE_PLAYLIST;
    BOOL downloads_page = g_page == PAGE_DOWNLOADS;
    BOOL settings_page = g_page == PAGE_SETTINGS;
    BOOL builtin_playlist = playlist_page && g_active_playlist >= 0 &&
                            g_active_playlist < g_playlist_count &&
                            g_playlists[g_active_playlist].is_builtin;

    ShowWindow(g_search_box, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_list, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_open_library, library_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_refresh, library_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_import_media, library_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_add_playlist, library_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_remove_playlist, playlist_page ? SW_SHOW : SW_HIDE);
    if (g_remove_playlist) SetWindowTextW(g_remove_playlist, builtin_playlist ? L"Unlike" : L"Remove");
    ShowWindow(g_edit_playlist, (playlist_page && !builtin_playlist) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_open_folder, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_delete_media, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_star_media, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_heart_media, (library_page || playlist_page) ? SW_SHOW : SW_HIDE);

    ShowWindow(g_url_box, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_format, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_download, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_resolution, (downloads_page && _wcsicmp(g_download_format, L"MP4") == 0) ? SW_SHOW : SW_HIDE);

    ShowWindow(g_output, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_refresh_outputs, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_app_id_edit, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_toggle, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_save, settings_page ? SW_SHOW : SW_HIDE);

    InvalidateRect(g_nav_library, NULL, FALSE);
    InvalidateRect(g_nav_downloads, NULL, FALSE);
    InvalidateRect(g_nav_settings, NULL, FALSE);
    InvalidateRect(g_main, NULL, FALSE);
}

static void set_page(enum AppPage page) {
    g_page = page;
    if (page != PAGE_PLAYLIST && g_playlist_list) {
        SendMessageW(g_playlist_list, LB_SETCURSEL, (WPARAM)-1, 0);
    }
    populate_list();
    update_page_visibility();
    layout_ui(g_main);
}

static void create_ui(HWND hwnd) {
    create_fonts();

    g_nav_library = make_button(hwnd, ID_NAV_LIBRARY, L"Library");
    g_nav_downloads = make_button(hwnd, ID_NAV_DOWNLOADS, L"Downloads");
    g_nav_settings = make_button(hwnd, ID_NAV_SETTINGS, L"Settings");

    g_playlist_list = CreateWindowExW(0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        0, 0, S(190), S(200), hwnd, (HMENU)(INT_PTR)ID_PLAYLIST_LIST, g_instance, NULL);
    SendMessageW(g_playlist_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    SetWindowTheme(g_playlist_list, L"DarkMode_Explorer", NULL);
    g_new_playlist = make_button(hwnd, ID_NEW_PLAYLIST, L"New playlist");

    g_open_library = make_button(hwnd, ID_OPEN_LIBRARY, L"Open folder");
    g_refresh = make_button(hwnd, ID_REFRESH, L"Refresh");
    g_import_media = make_button(hwnd, ID_IMPORT_MEDIA, L"Import media");
    g_add_playlist = make_button(hwnd, ID_ADD_PLAYLIST, L"Add to playlist");
    g_remove_playlist = make_button(hwnd, ID_REMOVE_PLAYLIST, L"Remove");
    g_edit_playlist = make_button(hwnd, ID_EDIT_PLAYLIST, L"Edit playlist");
    g_open_folder = make_button(hwnd, ID_OPEN_FOLDER, L"Show in folder");
    g_delete_media = make_button(hwnd, ID_DELETE_MEDIA, L"Delete");
    g_star_media = make_button(hwnd, ID_STAR_MEDIA, L"Star");
    g_heart_media = make_button(hwnd, ID_HEART_MEDIA, L"Like");

    g_url_box = CreateWindowExW(0, INPUT_CLASS, L"",
                                WS_CHILD | WS_VISIBLE,
                                0, 0, S(600), S(44), hwnd, NULL, g_instance, NULL);
    g_url_edit = CreateWindowExW(0, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                 S(42), S(7), S(536), S(30), g_url_box,
                                 (HMENU)(INT_PTR)ID_URL, g_instance, NULL);
    SendMessageW(g_url_edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, S(2)));
    SendMessageW(g_url_edit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)L"Paste a media link or playlist URL");
    g_old_url_edit_proc = (WNDPROC)SetWindowLongPtrW(g_url_edit, GWLP_WNDPROC, (LONG_PTR)url_edit_proc);
    SetWindowTheme(g_url_edit, L"DarkMode_CFD", NULL);

    g_format = make_button(hwnd, ID_FORMAT, L"MP3");
    g_resolution = make_button(hwnd, ID_RESOLUTION, L"Best quality");
    g_download = make_button(hwnd, ID_DOWNLOAD, L"Download");
    g_install_tools = make_button(hwnd, ID_INSTALL_TOOLS, L"Repair downloader");
    ShowWindow(g_install_tools, SW_HIDE);

    g_search_box = CreateWindowExW(0, SEARCH_CLASS, L"",
                                   WS_CHILD | WS_VISIBLE,
                                   0, 0, S(440), S(40), hwnd, NULL, g_instance, NULL);
    g_search = CreateWindowExW(0, L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                               S(38), S(6), S(388), S(28), g_search_box,
                               (HMENU)(INT_PTR)ID_SEARCH, g_instance, NULL);
    SendMessageW(g_search, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, S(2)));
    SendMessageW(g_search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search audio and video");
    g_old_search_edit_proc = (WNDPROC)SetWindowLongPtrW(g_search, GWLP_WNDPROC, (LONG_PTR)search_edit_proc);
    SetWindowTheme(g_search, L"DarkMode_CFD", NULL);

    g_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                             LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                             0, 0, S(800), S(400), hwnd,
                             (HMENU)(INT_PTR)ID_LIST, g_instance, NULL);
    ListView_SetExtendedListViewStyle(g_list,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ListView_SetBkColor(g_list, C_BG);
    ListView_SetTextBkColor(g_list, C_BG);
    ListView_SetTextColor(g_list, C_TEXT);
    SetWindowTheme(g_list, L"DarkMode_Explorer", NULL);
    SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    init_list_columns();

    g_list_header = ListView_GetHeader(g_list);
    SetWindowTheme(g_list_header, L"DarkMode_Explorer", NULL);
    SetWindowSubclass(g_list, list_subclass_proc, 1, 0);

    g_previous = make_button(hwnd, ID_PREVIOUS, L"");
    g_play = make_button(hwnd, ID_PLAY, L"");
    g_next = make_button(hwnd, ID_NEXT, L"");
    g_loop = make_button(hwnd, ID_LOOP, L"");
    g_shuffle = make_button(hwnd, ID_SHUFFLE, L"");

    g_seek = CreateWindowExW(0, SLIDER_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                             0, 0, S(420), S(24), hwnd,
                             (HMENU)(INT_PTR)ID_SEEK, g_instance, NULL);
    g_volume = CreateWindowExW(0, SLIDER_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                               0, 0, S(100), S(24), hwnd,
                               (HMENU)(INT_PTR)ID_VOLUME, g_instance, NULL);
    slider_set_value(g_seek, 0);
    slider_set_value(g_volume, g_volume_percent);

    g_output = CreateWindowExW(0, L"COMBOBOX", L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL,
                               0, 0, S(210), S(220), hwnd,
                               (HMENU)(INT_PTR)ID_OUTPUT, g_instance, NULL);
    SendMessageW(g_output, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    SetWindowTheme(g_output, L"DarkMode_CFD", NULL);
    g_refresh_outputs = make_button(hwnd, ID_REFRESH_OUTPUTS, L"Refresh devices");
    enumerate_audio_outputs();

    g_discord_app_id_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER,
        0, 0, S(320), S(38), hwnd, (HMENU)(INT_PTR)ID_DISCORD_APP_ID, g_instance, NULL);
    SendMessageW(g_discord_app_id_edit, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    SendMessageW(g_discord_app_id_edit, EM_SETLIMITTEXT, 24, 0);
    SendMessageW(g_discord_app_id_edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(S(10), S(10)));
    SendMessageW(g_discord_app_id_edit, EM_SETCUEBANNER, TRUE,
                 (LPARAM)L"Discord Application ID");
    SetWindowTheme(g_discord_app_id_edit, L"DarkMode_CFD", NULL);
    g_discord_toggle = make_button(hwnd, ID_DISCORD_TOGGLE, L"Discord presence: Off");
    g_discord_save = make_button(hwnd, ID_DISCORD_SAVE, L"Save & connect");

    g_video_window = CreateWindowExW(0, VIDEO_CLASS, L"Charter Video Player",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, S(960), S(600), hwnd, NULL, g_instance, NULL);

    update_page_visibility();
}

static void set_rounded_region(HWND hwnd, int width, int height, int radius) {
    if (!hwnd || width <= 0 || height <= 0) return;
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
    if (region) SetWindowRgn(hwnd, region, TRUE);
}

static void layout_ui(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int sidebar = S(220);
    int player_h = S(104);
    int player_top = h - player_h;
    int content_left = sidebar + S(28);
    int content_right = w - S(28);
    int content_w = max(S(420), content_right - content_left);

    MoveWindow(g_nav_library, S(14), S(82), sidebar - S(28), S(42), TRUE);
    MoveWindow(g_nav_downloads, S(14), S(130), sidebar - S(28), S(42), TRUE);
    MoveWindow(g_nav_settings, S(14), S(178), sidebar - S(28), S(42), TRUE);

    int playlist_top = S(276);
    int playlist_h = max(S(100), player_top - playlist_top - S(58));
    MoveWindow(g_playlist_list, S(14), playlist_top, sidebar - S(28), playlist_h, TRUE);
    MoveWindow(g_new_playlist, S(14), player_top - S(48), sidebar - S(28), S(36), TRUE);

    int header_y = S(92);
    int action_h = S(38);
    int gap = S(8);

    int top_actions_w = S(126 + 98 + 142) + gap * 3;
    int search_w = max(S(240), min(S(460), content_w - top_actions_w));
    MoveWindow(g_search_box, content_left, header_y, search_w, S(40), TRUE);

    int x = content_right;
    MoveWindow(g_open_library, x - S(126), header_y + S(1), S(126), action_h, TRUE); x -= S(126) + gap;
    MoveWindow(g_refresh, x - S(98), header_y + S(1), S(98), action_h, TRUE);
    x -= S(98) + gap;
    MoveWindow(g_import_media, x - S(142), header_y + S(1), S(142), action_h, TRUE);

    int action_x = content_right;
    MoveWindow(g_open_folder, action_x - S(142), S(143), S(142), action_h, TRUE); action_x -= S(142) + gap;
    MoveWindow(g_delete_media, action_x - S(108), S(143), S(108), action_h, TRUE); action_x -= S(108) + gap;
    MoveWindow(g_heart_media, action_x - S(92), S(143), S(92), action_h, TRUE); action_x -= S(92) + gap;
    MoveWindow(g_star_media, action_x - S(92), S(143), S(92), action_h, TRUE); action_x -= S(92) + gap;
    if (g_page == PAGE_PLAYLIST) {
        MoveWindow(g_remove_playlist, action_x - S(104), S(143), S(104), action_h, TRUE);
        action_x -= S(104) + gap;
        MoveWindow(g_edit_playlist, action_x - S(132), S(143), S(132), action_h, TRUE);
    } else {
        MoveWindow(g_add_playlist, action_x - S(154), S(143), S(154), action_h, TRUE);
    }

    int list_top = S(194);
    int list_h = max(S(220), player_top - list_top - S(16));
    MoveWindow(g_list, content_left, list_top, content_w, list_h, TRUE);
    set_rounded_region(g_list, content_w, list_h, S(8));
    update_list_columns();

    int dl_card_left = content_left;
    int dl_card_right = content_right;
    int dl_input_y = S(184);
    int dl_gap = S(10);
    int format_w = S(110);
    BOOL video_mode = _wcsicmp(g_download_format, L"MP4") == 0;
    int resolution_w = video_mode ? S(124) : 0;
    int download_w = S(128);
    int url_w = max(S(220), dl_card_right - dl_card_left - format_w - resolution_w -
                    download_w - dl_gap * (video_mode ? 3 : 2) - S(32));
    MoveWindow(g_url_box, dl_card_left + S(16), dl_input_y, url_w, S(44), TRUE);
    MoveWindow(g_format, dl_card_left + S(16) + url_w + dl_gap, dl_input_y, format_w, S(44), TRUE);
    int dl_x = dl_card_left + S(16) + url_w + dl_gap + format_w + dl_gap;
    MoveWindow(g_resolution, dl_x, dl_input_y, S(124), S(44), TRUE);
    if (video_mode) dl_x += S(124) + dl_gap;
    MoveWindow(g_download, dl_x,
               dl_input_y, download_w, S(44), TRUE);

    BOOL compact_player = w < S(1000);
    int center = compact_player ? sidebar + (w - sidebar) / 2 : w / 2;
    int controls_y = player_top + S(13);
    MoveWindow(g_previous, center - S(70), controls_y, S(36), S(36), TRUE);
    MoveWindow(g_play, center - S(22), controls_y - S(3), S(44), S(44), TRUE);
    MoveWindow(g_next, center + S(34), controls_y, S(36), S(36), TRUE);
    MoveWindow(g_loop, center - S(122), controls_y, S(36), S(36), TRUE);
    MoveWindow(g_shuffle, center + S(86), controls_y, S(36), S(36), TRUE);
    int seek_w = compact_player ? max(S(180), min(S(410), w - sidebar - S(100))) : S(410);
    int seek_x = compact_player ? center - seek_w / 2 : center - S(250);
    MoveWindow(g_seek, seek_x, player_top + S(61), seek_w, S(24), TRUE);
    ShowWindow(g_volume, compact_player ? SW_HIDE : SW_SHOW);
    if (!compact_player)
        MoveWindow(g_volume, center + S(220), player_top + S(61), S(118), S(24), TRUE);

    int output_w = min(S(520), content_w - S(220));
    MoveWindow(g_output, content_left + S(24), S(218), output_w, S(220), TRUE);
    MoveWindow(g_refresh_outputs, content_left + S(24) + output_w + gap, S(218), S(148), S(38), TRUE);

    int discord_inner_w = content_w - S(48);
    int discord_toggle_w = S(176);
    int discord_save_w = S(136);
    int discord_edit_w = max(S(220), discord_inner_w - discord_toggle_w - discord_save_w - gap * 2);
    int discord_x = content_left + S(24);
    MoveWindow(g_discord_app_id_edit, discord_x, S(454), discord_edit_w, S(38), TRUE);
    discord_x += discord_edit_w + gap;
    MoveWindow(g_discord_toggle, discord_x, S(454), discord_toggle_w, S(38), TRUE);
    discord_x += discord_toggle_w + gap;
    MoveWindow(g_discord_save, discord_x, S(454), discord_save_w, S(38), TRUE);

    update_page_visibility();
    InvalidateRect(hwnd, NULL, FALSE);
}

static void draw_text_line(HDC dc, const wchar_t *text, RECT rc, HFONT font,
                           COLORREF color, UINT format) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    SelectObject(dc, font);
    DrawTextW(dc, text ? text : L"", -1, &rc, format | DT_NOPREFIX);
}

static void format_time_100ns(LONGLONG value, wchar_t *out, size_t count) {
    if (value < 0) value = 0;
    long long seconds = value / 10000000LL;
    long long minutes = seconds / 60;
    seconds %= 60;
    swprintf(out, count, L"%lld:%02lld", minutes, seconds);
}

static void paint_main(HWND hwnd, HDC dc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    int sidebar = S(220);
    int player_h = S(104);
    int player_top = h - player_h;
    int content_left = sidebar + S(28);
    int content_right = w - S(28);

    HBRUSH bg = CreateSolidBrush(C_BG);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    RECT side = { 0, 0, sidebar, player_top };
    HBRUSH sb = CreateSolidBrush(C_SIDEBAR);
    FillRect(dc, &side, sb);
    DeleteObject(sb);

    draw_app_mark(dc, S(16), S(18), S(34));
    RECT app_title = { S(58), S(16), sidebar - S(10), S(42) };
    draw_text_line(dc, L"Charter", app_title, g_font_body_semibold, C_TEXT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT app_sub = { S(58), S(39), sidebar - S(10), S(59) };
    draw_text_line(dc, L"Music Browser", app_sub, g_font_small, C_TEXT_DIM,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT pl_label = { S(18), S(238), sidebar - S(18), S(264) };
    draw_text_line(dc, L"PLAYLISTS", pl_label, g_font_small_semibold, C_TEXT_FAINT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const wchar_t *page_title = L"Library";
    const wchar_t *page_sub = L"Everything saved in Charter Music Browser";
    if (g_page == PAGE_DOWNLOADS) {
        page_title = L"Add music";
        page_sub = L"Download audio or MP4 video directly into your library";
    } else if (g_page == PAGE_PLAYLIST) {
        if (g_active_playlist >= 0 && g_active_playlist < g_playlist_count) page_title = g_playlists[g_active_playlist].name;
        else page_title = L"Playlist";
        page_sub = L"Your saved playlist";
    } else if (g_page == PAGE_SETTINGS) {
        page_title = L"Settings";
        page_sub = L"Playback, audio, and Discord presence";
    }

    RECT title_rc = { content_left, S(20), content_right, S(54) };
    draw_text_line(dc, page_title, title_rc, g_font_title, C_TEXT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT sub_rc = { content_left, S(52), content_right, S(74) };
    draw_text_line(dc, page_sub, sub_rc, g_font_small, C_TEXT_DIM,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (g_page == PAGE_LIBRARY || g_page == PAGE_PLAYLIST) {
        wchar_t count_text[128];
        if (g_visible_count != g_track_count && g_page == PAGE_LIBRARY)
            swprintf(count_text, ARRAY_LEN(count_text), L"%zu shown", g_visible_count);
        else
            swprintf(count_text, ARRAY_LEN(count_text), L"%zu media item%ls", g_visible_count, g_visible_count == 1 ? L"" : L"s");
        RECT count_rc = { content_left, S(145), content_left + S(180), S(176) };
        draw_text_line(dc, count_text, count_rc, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    if (g_page == PAGE_DOWNLOADS) {
        RECT card = { content_left, S(112), content_right, min(player_top - S(24), S(330)) };
        fill_round_rect(dc, card, S(12), C_CARD);
        stroke_round_rect(dc, card, S(12), C_BORDER_SOFT);
        RECT dl_title = { card.left + S(16), card.top + S(14), card.right - S(16), card.top + S(42) };
        draw_text_line(dc, L"Download into Charter", dl_title, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT dl_sub = { card.left + S(16), card.top + S(42), card.right - S(16), card.top + S(68) };
        draw_text_line(dc, L"Paste any media link the downloader can resolve. Audio or video is saved directly into your library.",
                       dl_sub, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        wchar_t tools_text[256];
        refresh_tool_paths();
        BOOL core_ready = g_ytdlp_path[0] && g_qjs_path[0];
        if (InterlockedCompareExchange(&g_installing_tools, 0, 0)) wcscpy(tools_text, L"Preparing downloader...");
        else if (core_ready) wcscpy(tools_text, L"Downloader ready");
        else wcscpy(tools_text, L"Downloader prepares itself on first use");
        RECT tools_rc = { card.left + S(16), S(240), card.right - S(16), S(264) };
        draw_text_line(dc, tools_text, tools_rc, g_font_small, core_ready ? C_TEXT_DIM : C_ACCENT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT status_rc = { card.left + S(16), S(268), card.right - S(16), S(294) };
        draw_text_line(dc, g_download_status, status_rc, g_font_small,
                       InterlockedCompareExchange(&g_downloading, 0, 0) ? C_TEXT : C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT progress_bg = { card.left + S(16), card.bottom - S(12), card.right - S(16), card.bottom - S(7) };
        fill_round_rect(dc, progress_bg, S(3), C_SURFACE);
        if (g_download_percent >= 0) {
            int pct = min(100, max(0, g_download_percent));
            RECT progress = progress_bg;
            progress.right = progress.left + MulDiv(progress_bg.right - progress_bg.left, pct, 100);
            if (progress.right > progress.left) fill_round_rect(dc, progress, S(3), C_ACCENT);
        }
    }

    if (g_page == PAGE_SETTINGS) {
        RECT card = { content_left, S(112), content_right, min(player_top - S(24), S(330)) };
        fill_round_rect(dc, card, S(12), C_CARD);
        stroke_round_rect(dc, card, S(12), C_BORDER_SOFT);
        RECT heading = { card.left + S(20), card.top + S(16), card.right - S(20), card.top + S(46) };
        draw_text_line(dc, L"Audio output", heading, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT info = { card.left + S(20), card.top + S(47), card.right - S(20), card.top + S(76) };
        draw_text_line(dc, L"Choose where Charter plays audio. Changes apply immediately to the current track.",
                       info, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT device_label = { card.left + S(24), card.top + S(78), card.left + S(270), card.top + S(104) };
        draw_text_line(dc, L"Output device", device_label, g_font_small_semibold, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT discord_card = { content_left, S(350), content_right,
                              min(player_top - S(24), S(570)) };
        fill_round_rect(dc, discord_card, S(12), C_CARD);
        stroke_round_rect(dc, discord_card, S(12), C_BORDER_SOFT);
        RECT discord_heading = { discord_card.left + S(20), discord_card.top + S(16),
                                 discord_card.right - S(20), discord_card.top + S(46) };
        draw_text_line(dc, L"Discord Rich Presence", discord_heading, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT discord_info = { discord_card.left + S(20), discord_card.top + S(47),
                              discord_card.right - S(20), discord_card.top + S(76) };
        draw_text_line(dc, L"Optionally show the current title, artist, media type, and play/pause state to your Discord friends.",
                       discord_info, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT app_id_label = { discord_card.left + S(24), discord_card.top + S(78),
                              discord_card.right - S(24), discord_card.top + S(104) };
        draw_text_line(dc, L"Discord Application ID", app_id_label, g_font_small_semibold, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT discord_status = { discord_card.left + S(24), discord_card.top + S(148),
                                discord_card.right - S(24), discord_card.top + S(174) };
        draw_text_line(dc, g_discord_status, discord_status, g_font_small,
                       g_discord_enabled ? C_ACCENT : C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT discord_hint = { discord_card.left + S(24), discord_card.top + S(176),
                              discord_card.right - S(24), discord_card.bottom - S(10) };
        draw_text_line(dc, L"Create an application in the Discord Developer Portal, then paste its public Application ID here.",
                       discord_hint, g_font_small, C_TEXT_FAINT,
                       DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    RECT player = { 0, player_top, w, h };
    HBRUSH pb = CreateSolidBrush(C_PLAYER);
    FillRect(dc, &player, pb);
    DeleteObject(pb);
    draw_line(dc, 0, player_top, w, player_top, C_PLAYER_TOP, 1);

    RECT art = { S(14), player_top + S(16), S(82), player_top + S(84) };
    fill_round_rect(dc, art, S(7), C_SURFACE);

    Track *playing = g_current_track_index < g_track_count ? &g_tracks[g_current_track_index] : NULL;
    if (playing && g_track_images && playing->image_index >= 0) {
        HICON cover = ImageList_GetIcon(g_track_images, playing->image_index, ILD_NORMAL);
        if (cover) {
            DrawIconEx(dc, art.left + S(4), art.top + S(4), cover,
                       S(60), S(60), 0, NULL, DI_NORMAL);
            DestroyIcon(cover);
        }
    } else if (g_icon_musical || g_app_icon) {
        HICON fallback = g_icon_musical ? g_icon_musical : g_app_icon;
        DrawIconEx(dc, art.left + S(8), art.top + S(8), fallback,
                   S(52), S(52), 0, NULL, DI_NORMAL);
    }
    RECT np_title = { S(94), player_top + S(19), min(w / 2 - S(250), S(390)), player_top + S(45) };
    draw_text_line(dc, playing ? playing->title : L"Nothing playing", np_title,
                   g_font_body_semibold, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT np_sub = { S(94), player_top + S(47), min(w / 2 - S(250), S(390)), player_top + S(72) };
    draw_text_line(dc, playing ? playing->artist : (g_status_text[0] ? g_status_text : L"Choose a track from your library"),
                   np_sub, g_font_small, C_TEXT_DIM,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    BOOL compact_player = w < S(1000);
    int center = compact_player ? sidebar + (w - sidebar) / 2 : w / 2;
    int seek_w = compact_player ? max(S(180), min(S(410), w - sidebar - S(100))) : S(410);
    int seek_left = compact_player ? center - seek_w / 2 : center - S(250);
    int seek_right = seek_left + seek_w;
    LONGLONG pos = player_position_100ns();
    LONGLONG dur = player_duration_100ns();
    wchar_t left_time[32], right_time[32];
    format_time_100ns(pos, left_time, ARRAY_LEN(left_time));
    format_time_100ns(dur, right_time, ARRAY_LEN(right_time));
    RECT time_l = { seek_left - S(52), player_top + S(64), seek_left - S(6), player_top + S(88) };
    RECT time_r = { seek_right + S(6), player_top + S(64), seek_right + S(52), player_top + S(88) };
    draw_text_line(dc, left_time, time_l, g_font_small, C_TEXT_FAINT,
                   DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    draw_text_line(dc, right_time, time_r, g_font_small, C_TEXT_FAINT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (!compact_player) {
        RECT vol_label = { center + S(342), player_top + S(60), center + S(398), player_top + S(84) };
        wchar_t vol_text[32];
        swprintf(vol_text, ARRAY_LEN(vol_text), L"%d%%", g_volume_percent);
        draw_text_line(dc, vol_text, vol_label, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static LRESULT list_custom_draw(NMLVCUSTOMDRAW *cd) {
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) {
        return CDRF_NOTIFYITEMDRAW;
    }

    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        BOOL selected = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
        BOOL hot = (cd->nmcd.uItemState & CDIS_HOT) != 0;
        cd->clrText = C_TEXT;
        cd->clrTextBk = selected ? C_SELECTED : (hot ? C_CARD_ALT : C_BG);
        return CDRF_NEWFONT;
    }

    return CDRF_DODEFAULT;
}

static void update_button_enabled_state(void) {
    BOOL has_selection = selected_track() != NULL;
    BOOL downloading = InterlockedCompareExchange(&g_downloading, 0, 0) != 0;
    BOOL installing = InterlockedCompareExchange(&g_installing_tools, 0, 0) != 0;

    refresh_tool_paths();

    if (g_download) SetWindowTextW(g_download, downloading ? L"Cancel" : (installing ? L"Preparing..." : L"Download"));

    if (g_play) EnableWindow(g_play, has_selection || g_current_track_index < g_track_count);
    if (g_previous) EnableWindow(g_previous, ListView_GetItemCount(g_list) > 0);
    if (g_next) EnableWindow(g_next, ListView_GetItemCount(g_list) > 0);
    if (g_open_folder) EnableWindow(g_open_folder, has_selection);
    if (g_add_playlist) EnableWindow(g_add_playlist, has_selection);
    if (g_delete_media) EnableWindow(g_delete_media, has_selection);
    if (g_star_media) EnableWindow(g_star_media, has_selection);
    if (g_heart_media) EnableWindow(g_heart_media, has_selection);
    if (g_remove_playlist) EnableWindow(g_remove_playlist, has_selection && g_page == PAGE_PLAYLIST);
    if (g_edit_playlist) EnableWindow(g_edit_playlist, g_active_playlist >= 0 &&
        g_page == PAGE_PLAYLIST && !g_playlists[g_active_playlist].is_builtin);
    if (g_open_library) EnableWindow(g_open_library, g_library_ready);
    if (g_refresh) EnableWindow(g_refresh, g_library_ready);
    if (g_import_media) EnableWindow(g_import_media, g_library_ready);

    if (g_url_edit) EnableWindow(g_url_edit, !downloading && !installing);
    if (g_format) EnableWindow(g_format, !downloading && !installing);
    if (g_resolution) EnableWindow(g_resolution, !downloading && !installing);
    if (g_download) EnableWindow(g_download, g_library_ready && !installing);
    if (g_install_tools) EnableWindow(g_install_tools, !downloading && !installing);
    if (g_output) EnableWindow(g_output, TRUE);
    if (g_discord_app_id_edit) EnableWindow(g_discord_app_id_edit, TRUE);
    if (g_discord_toggle) EnableWindow(g_discord_toggle, TRUE);
    if (g_discord_save) EnableWindow(g_discord_save, TRUE);

    if (g_play) InvalidateRect(g_play, NULL, FALSE);
    if (g_star_media) InvalidateRect(g_star_media, NULL, FALSE);
    if (g_heart_media) InvalidateRect(g_heart_media, NULL, FALSE);
    if (g_loop) InvalidateRect(g_loop, NULL, FALSE);
    if (g_shuffle) InvalidateRect(g_shuffle, NULL, FALSE);
    if (g_discord_toggle) InvalidateRect(g_discord_toggle, NULL, FALSE);
}

static void paint_owner_draw_item(const DRAWITEMSTRUCT *dis) {
    if (!dis || dis->itemID == (UINT)-1) return;
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    BOOL selected = (dis->itemState & ODS_SELECTED) != 0;
    BOOL focused = (dis->itemState & ODS_FOCUS) != 0;
    COLORREF bg = C_SIDEBAR;
    COLORREF fg = C_TEXT_DIM;

    if (dis->CtlID == ID_OUTPUT) {
        bg = selected ? C_SURFACE_HOVER : C_SURFACE;
        fg = C_TEXT;
    } else if (dis->CtlID == ID_PLAYLIST_LIST) {
        bg = selected ? C_SURFACE : C_SIDEBAR;
        fg = selected ? C_TEXT : C_TEXT_DIM;
    }

    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);

    wchar_t text[256] = L"";
    if (dis->CtlID == ID_OUTPUT) {
        SendMessageW(dis->hwndItem, CB_GETLBTEXT, dis->itemID, (LPARAM)text);
    } else {
        SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
    }

    RECT tr = rc;
    if (dis->CtlID == ID_PLAYLIST_LIST) {
        int icon_size = S(36);
        int ix = rc.left + S(10);
        int iy = rc.top + ((rc.bottom - rc.top) - icon_size) / 2;
        if ((int)dis->itemID < g_playlist_count && g_playlists[dis->itemID].artwork) {
            BITMAP bm;
            ZeroMemory(&bm, sizeof(bm));
            GetObject(g_playlists[dis->itemID].artwork, sizeof(bm), &bm);
            HDC mem = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mem, g_playlists[dis->itemID].artwork);
            BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            if (!AlphaBlend(dc, ix, iy, icon_size, icon_size, mem, 0, 0,
                            max(1, bm.bmWidth), max(1, abs(bm.bmHeight)), blend)) {
                SetStretchBltMode(dc, HALFTONE);
                StretchBlt(dc, ix, iy, icon_size, icon_size, mem, 0, 0,
                           max(1, bm.bmWidth), max(1, abs(bm.bmHeight)), SRCCOPY);
            }
            SelectObject(mem, old);
            DeleteDC(mem);
        } else if ((int)dis->itemID < g_playlist_count && g_playlists[dis->itemID].is_builtin && g_icon_heart) {
            DrawIconEx(dc, ix, iy, g_icon_heart, icon_size, icon_size, 0, NULL, DI_NORMAL);
        } else if (g_app_icon) {
            DrawIconEx(dc, ix, iy, g_app_icon, icon_size, icon_size, 0, NULL, DI_NORMAL);
        }
        tr.left = ix + icon_size + S(10);
    } else {
        tr.left += S(10);
    }
    tr.right -= S(8);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    SelectObject(dc, dis->CtlID == ID_OUTPUT ? g_font_small : g_font_body);
    DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    if (dis->CtlID == ID_PLAYLIST_LIST && selected) {
        RECT mark = { rc.left + S(2), rc.top + S(7), rc.left + S(5), rc.bottom - S(7) };
        fill_round_rect(dc, mark, S(2), C_ACCENT);
    }
    if (focused && dis->CtlID == ID_OUTPUT) {
        RECT focus = rc;
        InflateRect(&focus, -S(1), -S(1));
        stroke_round_rect(dc, focus, S(5), C_ACCENT);
    }
}

static void apply_windows_11_window_style(HWND hwnd) {
    BOOL dark = TRUE;
    HRESULT hr = DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    if (FAILED(hr)) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }

    int corner_preference = 2; /* DWMWCP_ROUND */
    DwmSetWindowAttribute(hwnd, 33, &corner_preference, sizeof(corner_preference));
}

static void discord_read_app_id_field(wchar_t *output, size_t output_count) {
    if (!output || output_count == 0) return;
    output[0] = L'\0';
    if (!g_discord_app_id_edit) return;
    wchar_t raw[64] = L"";
    GetWindowTextW(g_discord_app_id_edit, raw, ARRAY_LEN(raw));
    wchar_t *start = raw;
    while (*start && iswspace(*start)) ++start;
    wchar_t *end = start + wcslen(start);
    while (end > start && iswspace(end[-1])) --end;
    *end = L'\0';
    wcsncpy(output, start, output_count - 1);
    output[output_count - 1] = L'\0';
}

static void save_discord_settings_from_ui(void) {
    wchar_t app_id[ARRAY_LEN(g_discord_app_id)];
    discord_read_app_id_field(app_id, ARRAY_LEN(app_id));
    BOOL changed = wcscmp(app_id, g_discord_app_id) != 0;
    if (changed) discord_disconnect(TRUE);
    wcsncpy(g_discord_app_id, app_id, ARRAY_LEN(g_discord_app_id) - 1);
    g_discord_app_id[ARRAY_LEN(g_discord_app_id) - 1] = L'\0';
    SetWindowTextW(g_discord_app_id_edit, g_discord_app_id);

    if (!save_discord_config()) {
        discord_set_status(L"Could not save Discord settings.");
    } else if (!g_discord_enabled) {
        discord_set_status(L"Saved. Discord presence is off.");
    } else if (!discord_app_id_valid(g_discord_app_id)) {
        discord_disconnect(FALSE);
        discord_set_status(L"Enter a valid numeric Discord Application ID.");
    } else {
        g_discord_last_attempt = 0;
        discord_mark_dirty();
        discord_set_status(L"Waiting for the Discord desktop app...");
        discord_tick();
    }
    update_button_enabled_state();
}

static void toggle_discord_presence(void) {
    g_discord_enabled = !g_discord_enabled;
    update_discord_toggle_label();
    if (!g_discord_enabled) {
        discord_disconnect(TRUE);
        discord_set_status(L"Disabled");
    } else {
        wchar_t app_id[ARRAY_LEN(g_discord_app_id)];
        discord_read_app_id_field(app_id, ARRAY_LEN(app_id));
        wcsncpy(g_discord_app_id, app_id, ARRAY_LEN(g_discord_app_id) - 1);
        g_discord_app_id[ARRAY_LEN(g_discord_app_id) - 1] = L'\0';
        if (!discord_app_id_valid(g_discord_app_id)) {
            discord_set_status(L"Enter a valid numeric Discord Application ID.");
        } else {
            g_discord_last_attempt = 0;
            discord_mark_dirty();
            discord_set_status(L"Waiting for the Discord desktop app...");
            discord_tick();
        }
    }
    save_discord_config();
    update_button_enabled_state();
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            g_main = hwnd;
            g_dpi = GetDpiForWindow(hwnd);
            if (!g_dpi) g_dpi = 96;
            apply_windows_11_window_style(hwnd);
            create_ui(hwnd);
            if (initialize_library_path()) {
                refresh_library();
            } else {
                set_status(L"Could not create the Charter music library in AppData.");
            }
            SetTimer(hwnd, 1, 250, NULL);
            update_button_enabled_state();
            return 0;

        case WM_SIZE:
            layout_ui(hwnd);
            return 0;

        case WM_DPICHANGED: {
            UINT new_dpi = HIWORD(wParam);
            if (!new_dpi) new_dpi = LOWORD(wParam);
            if (new_dpi) g_dpi = new_dpi;
            RECT *suggested = (RECT *)lParam;
            SetWindowPos(hwnd, NULL,
                         suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            create_fonts();
            SendMessageW(g_playlist_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
            SendMessageW(g_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
            SendMessageW(g_output, WM_SETFONT, (WPARAM)g_font_small, TRUE);
            layout_ui(hwnd);
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);
            if (id == ID_NAV_LIBRARY && code == BN_CLICKED) {
                set_page(PAGE_LIBRARY);
            } else if (id == ID_NAV_DOWNLOADS && code == BN_CLICKED) {
                set_page(PAGE_DOWNLOADS);
            } else if (id == ID_NAV_SETTINGS && code == BN_CLICKED) {
                set_page(PAGE_SETTINGS);
            } else if (id == ID_PLAYLIST_LIST && code == LBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_playlist_list, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_playlist_count) {
                    g_active_playlist = sel;
                    load_active_playlist();
                    set_page(PAGE_PLAYLIST);
                }
            } else if (id == ID_NEW_PLAYLIST && code == BN_CLICKED) {
                if (show_playlist_editor(-1)) {
                    load_active_playlist();
                    set_page(PAGE_PLAYLIST);
                    set_status(L"Created a new playlist.");
                }
            } else if (id == ID_EDIT_PLAYLIST && code == BN_CLICKED) {
                if (g_active_playlist >= 0 && show_playlist_editor(g_active_playlist)) {
                    set_page(PAGE_PLAYLIST);
                    set_status(L"Playlist updated.");
                }
            } else if (id == ID_ADD_PLAYLIST && code == BN_CLICKED) {
                show_add_to_playlist_menu();
                update_button_enabled_state();
            } else if (id == ID_REMOVE_PLAYLIST && code == BN_CLICKED) {
                Track *t = selected_track();
                if (t && g_active_playlist >= 0 && g_playlists[g_active_playlist].is_builtin) {
                    toggle_selected_heart();
                } else if (t && remove_path_from_active_playlist(t->path)) {
                    populate_list();
                    set_status(L"Removed the track from this playlist.");
                }
                update_button_enabled_state();
            } else if (id == ID_OPEN_LIBRARY && code == BN_CLICKED) {
                open_library_folder();
                update_button_enabled_state();
            } else if (id == ID_REFRESH && code == BN_CLICKED) {
                refresh_library();
                update_button_enabled_state();
            } else if (id == ID_IMPORT_MEDIA && code == BN_CLICKED) {
                import_media_files();
                update_button_enabled_state();
            } else if (id == ID_DELETE_MEDIA && code == BN_CLICKED) {
                delete_selected_media();
                update_button_enabled_state();
            } else if (id == ID_STAR_MEDIA && code == BN_CLICKED) {
                toggle_selected_star();
                update_button_enabled_state();
            } else if (id == ID_HEART_MEDIA && code == BN_CLICKED) {
                toggle_selected_heart();
                update_button_enabled_state();
            } else if (id == ID_DOWNLOAD && code == BN_CLICKED) {
                start_download();
                update_button_enabled_state();
            } else if (id == ID_FORMAT && code == BN_CLICKED) {
                show_format_menu();
                update_button_enabled_state();
            } else if (id == ID_RESOLUTION && code == BN_CLICKED) {
                show_resolution_menu();
                update_button_enabled_state();
            } else if (id == ID_INSTALL_TOOLS && code == BN_CLICKED) {
                start_install_tools();
                update_button_enabled_state();
            } else if (id == ID_SEARCH && code == EN_CHANGE) {
                populate_list();
                update_button_enabled_state();
            } else if (id == ID_PLAY && code == BN_CLICKED) {
                pause_resume();
                update_button_enabled_state();
            } else if (id == ID_PREVIOUS && code == BN_CLICKED) {
                play_previous_track();
                update_button_enabled_state();
            } else if (id == ID_NEXT && code == BN_CLICKED) {
                play_next_track();
                update_button_enabled_state();
            } else if (id == ID_LOOP && code == BN_CLICKED) {
                g_loop_enabled = !g_loop_enabled;
                set_status(g_loop_enabled ? L"Loop is on." : L"Loop is off.");
                update_button_enabled_state();
            } else if (id == ID_SHUFFLE && code == BN_CLICKED) {
                g_shuffle_enabled = !g_shuffle_enabled;
                set_status(g_shuffle_enabled ? L"Shuffle is on." : L"Shuffle is off.");
                update_button_enabled_state();
            } else if (id == ID_OPEN_FOLDER && code == BN_CLICKED) {
                open_selected_folder();
            } else if (id == ID_REFRESH_OUTPUTS && code == BN_CLICKED) {
                enumerate_audio_outputs();
                set_status(L"Audio output devices refreshed.");
            } else if (id == ID_DISCORD_TOGGLE && code == BN_CLICKED) {
                toggle_discord_presence();
            } else if (id == ID_DISCORD_SAVE && code == BN_CLICKED) {
                save_discord_settings_from_ui();
            } else if (id == ID_OUTPUT && code == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(g_output, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_audio_output_count && sel != g_audio_output_index) {
                    g_audio_output_index = sel;
                    restart_on_selected_output();
                    set_status(L"Audio output changed.");
                }
            }
            return 0;
        }

        case WM_APP_SLIDER_CHANGED:
            if ((int)wParam == ID_VOLUME) {
                g_volume_percent = (int)lParam;
                apply_player_volume();
                RECT rc;
                GetClientRect(hwnd, &rc);
                rc.top = max(0, rc.bottom - S(110));
                InvalidateRect(hwnd, &rc, FALSE);
            } else if (((int)wParam == ID_SEEK || (int)wParam == ID_VIDEO_SEEK) &&
                       g_current_track_index < g_track_count) {
                LONGLONG duration = player_duration_100ns();
                if (duration > 0) {
                    LONGLONG target = (duration * (LONGLONG)(int)lParam) / 1000LL;
                    player_seek_to(target);
                }
            }
            return 0;

        case WM_APP_AUDIO_COMPLETE: {
            BOOL failed = wParam != 0;
            if (failed) {
                release_graph();
                g_is_playing = FALSE;
                g_is_paused = FALSE;
                discord_mark_dirty();
                set_status(L"Charter could not decode this track with the Windows Media Foundation audio engine.");
            } else if (g_current_track_index < g_track_count) {
                play_after_completion();
            }
            update_button_enabled_state();
            return 0;
        }

        case WM_APP_VIDEO_COMPLETE: {
            BOOL failed = wParam != 0;
            if (failed) {
                release_graph();
                g_is_playing = FALSE;
                g_is_paused = FALSE;
                discord_mark_dirty();
                set_status(L"Charter could not decode this video. Try MP4 (H.264/AAC) or install the needed Windows codec.");
            } else if (g_current_track_index < g_track_count) {
                play_after_completion();
            }
            update_button_enabled_state();
            return 0;
        }

        case WM_TIMER:
            if (wParam == 1) {
                if (!g_seek_dragging && g_current_track_index < g_track_count) {
                    LONGLONG duration = player_duration_100ns();
                    LONGLONG pos = player_position_100ns();
                    if (duration > 0) {
                        int value = (int)((pos * 1000LL) / duration);
                        slider_set_value(g_seek, value);
                        if (g_video_seek) slider_set_value(g_video_seek, value);
                    }
                }
                RECT player_rc;
                GetClientRect(hwnd, &player_rc);
                player_rc.top = max(0, player_rc.bottom - S(110));
                InvalidateRect(hwnd, &player_rc, FALSE);
                discord_tick();
            }
            return 0;

        case WM_APP_DOWNLOAD_UPDATE: {
            DownloadUpdate *update = (DownloadUpdate *)lParam;
            if (update) {
                int pct = update->percent;
                if (pct < 0 && InterlockedCompareExchange(&g_downloading, 0, 0)) pct = g_download_percent;
                set_download_status(update->text, pct);
                free(update);
            }
            return 0;
        }

        case WM_APP_DOWNLOAD_DONE: {
            DWORD exit_code = (DWORD)wParam;
            if (g_download_process) { CloseHandle(g_download_process); g_download_process = NULL; }
            if (g_download_thread) { CloseHandle(g_download_thread); g_download_thread = NULL; }
            InterlockedExchange(&g_downloading, 0);

            if (exit_code == 0) {
                set_download_status(L"Download complete. The Charter library has been refreshed.", 100);
                refresh_library();
            } else if (exit_code == 2) {
                set_download_status(L"Download cancelled.", -1);
            } else if (g_last_download_error[0]) {
                set_download_status(g_last_download_error, -1);
            } else {
                set_download_status(L"The source could not be downloaded. Try another link or try again later.", -1);
            }
            update_button_enabled_state();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_APP_TOOLS_DONE: {
            DWORD failed = (DWORD)wParam;
            if (g_tools_thread) { CloseHandle(g_tools_thread); g_tools_thread = NULL; }
            InterlockedExchange(&g_installing_tools, 0);
            refresh_tool_paths();

            BOOL pending = InterlockedExchange(&g_pending_download, 0) != 0;
            BOOL pending_playback = InterlockedExchange(&g_pending_playback, 0) != 0;
            BOOL need_ffmpeg = InterlockedExchange(&g_pending_need_ffmpeg, 0) != 0;
            BOOL ready = g_ytdlp_path[0] && g_qjs_path[0] && (!need_ffmpeg || g_ffmpeg_path[0]);
            if (!failed && ready) {
                set_download_status(L"Downloader ready.", -1);
                update_button_enabled_state();
                InvalidateRect(hwnd, NULL, FALSE);
                if (pending) start_download();
                if (pending_playback && g_pending_playback_index < g_track_count) {
                    play_track_index_at(g_pending_playback_index, g_pending_playback_position,
                                        g_pending_playback_paused);
                }
            } else {
                g_pending_playback_index = (size_t)-1;
                set_download_status(L"Charter could not prepare the downloader automatically. Check your internet connection and try again.", -1);
                if (pending_playback) set_status(L"Charter could not prepare the compatibility decoder for this media file.");
                update_button_enabled_state();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_NOTIFY: {
            NMHDR *hdr = (NMHDR *)lParam;
            if (!hdr) return 0;
            if (hdr->idFrom == ID_LIST) {
                if (hdr->code == NM_DBLCLK) {
                    play_selected();
                    update_button_enabled_state();
                    return 0;
                }
                if (hdr->code == LVN_ITEMCHANGED) {
                    update_button_enabled_state();
                    return 0;
                }
                if (hdr->code == NM_CUSTOMDRAW) return list_custom_draw((NMLVCUSTOMDRAW *)lParam);
            }
            return 0;
        }

        case WM_MEASUREITEM: {
            MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
            if (mis->CtlID == ID_PLAYLIST_LIST) { mis->itemHeight = S(46); return TRUE; }
            if (mis->CtlID == ID_OUTPUT) { mis->itemHeight = S(30); return TRUE; }
            break;
        }

        case WM_DRAWITEM: {
            DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
            if (dis && (dis->CtlID == ID_PLAYLIST_LIST || dis->CtlID == ID_OUTPUT)) {
                paint_owner_draw_item(dis);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)wParam;
            HWND control = (HWND)lParam;
            SetBkMode(dc, TRANSPARENT);
            if (control == g_playlist_list) {
                SetBkColor(dc, C_SIDEBAR);
                SetTextColor(dc, C_TEXT_DIM);
            } else {
                SetBkColor(dc, C_SURFACE);
                SetTextColor(dc, C_TEXT);
            }
            return (LRESULT)(control == g_playlist_list ? g_sidebar_brush : g_search_brush);
        }

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wParam;
            HWND control = (HWND)lParam;
            if (control == g_discord_app_id_edit) {
                SetBkMode(dc, OPAQUE);
                SetBkColor(dc, C_SURFACE);
                SetTextColor(dc, C_TEXT);
                return (LRESULT)g_search_brush;
            }
            break;
        }

        case WM_KEYDOWN:
            if (wParam == VK_DELETE && GetFocus() != g_search && GetFocus() != g_url_edit &&
                GetFocus() != g_discord_app_id_edit) {
                delete_selected_media();
                update_button_enabled_state();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'O') {
                open_library_folder();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'I') {
                import_media_files();
                update_button_enabled_state();
                return 0;
            }
            if (wParam == VK_F5) {
                refresh_library();
                update_button_enabled_state();
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'F') {
                set_page(PAGE_LIBRARY);
                SetFocus(g_search);
                SendMessageW(g_search, EM_SETSEL, 0, -1);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'D') {
                set_page(PAGE_DOWNLOADS);
                SetFocus(g_url_edit);
                SendMessageW(g_url_edit, EM_SETSEL, 0, -1);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == '1') { set_page(PAGE_LIBRARY); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == '2') { set_page(PAGE_DOWNLOADS); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == '3') { set_page(PAGE_SETTINGS); return 0; }
            if (wParam == VK_SPACE && GetFocus() != g_search && GetFocus() != g_url_edit &&
                GetFocus() != g_discord_app_id_edit) {
                pause_resume();
                update_button_enabled_state();
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = max(1, rc.right - rc.left);
            int height = max(1, rc.bottom - rc.top);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
            HGDIOBJ old = SelectObject(mem, bitmap);
            paint_main(hwnd, mem);
            BitBlt(dc, 0, 0, width, height, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bitmap);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            mmi->ptMinTrackSize.x = S(1020);
            mmi->ptMinTrackSize.y = S(700);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            discord_disconnect(TRUE);
            stop_playback();
            if (g_video_window) {
                DestroyWindow(g_video_window);
                g_video_window = NULL;
            }
            free_audio_outputs();
            free_playlist_tracks();
            if (g_download_process) {
                TerminateProcess(g_download_process, 2);
                CloseHandle(g_download_process);
                g_download_process = NULL;
            }
            if (g_download_thread) { CloseHandle(g_download_thread); g_download_thread = NULL; }
            if (g_tools_thread) { CloseHandle(g_tools_thread); g_tools_thread = NULL; }
            free_tracks();
            free_media_flags();
            if (g_track_images) {
                ImageList_Destroy(g_track_images);
                g_track_images = NULL;
            }
            for (int i = 0; i < g_playlist_count; ++i) {
                if (g_playlists[i].artwork) {
                    DeleteObject(g_playlists[i].artwork);
                    g_playlists[i].artwork = NULL;
                }
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

