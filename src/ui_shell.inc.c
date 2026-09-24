/* Playlist dialog, main layout, painting, and window procedure. */

static void init_list_columns(void) {
    static const wchar_t *labels[] = { L"Icon", L"Name", L"Artist", L"Actions" };
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        col.pszText = (wchar_t *)labels[i];
        col.cx = S(120);
        col.iSubItem = i;
        col.fmt = LVCFMT_LEFT;
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
                S(24), S(52), S(350), S(28), hwnd, (HMENU)(INT_PTR)ID_PL_NAME_EDIT, g_instance, NULL);
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
    ShowWindow(g_add_playlist, SW_HIDE);
    ShowWindow(g_remove_playlist, (playlist_page && !builtin_playlist) ? SW_SHOW : SW_HIDE);
    if (g_remove_playlist) SetWindowTextW(g_remove_playlist, L"Delete playlist");
    ShowWindow(g_edit_playlist, (playlist_page && !builtin_playlist) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_open_folder, SW_HIDE);
    ShowWindow(g_delete_media, SW_HIDE);
    ShowWindow(g_star_media, SW_HIDE);
    ShowWindow(g_heart_media, SW_HIDE);

    ShowWindow(g_url_box, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_format, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_download, downloads_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_resolution, (downloads_page && _wcsicmp(g_download_format, L"MP4") == 0) ? SW_SHOW : SW_HIDE);

    ShowWindow(g_output, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_refresh_outputs, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_app_id_edit,
               (settings_page && !g_discord_use_provided) ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_mode, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_toggle, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_discord_save, settings_page ? SW_SHOW : SW_HIDE);
    ShowWindow(g_migrate_storage, settings_page ? SW_SHOW : SW_HIDE);

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
    g_remove_playlist = make_button(hwnd, ID_REMOVE_PLAYLIST, L"Delete playlist");
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
                             LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
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
    g_discord_mode = make_button(hwnd, ID_DISCORD_MODE, L"Provided ID");
    g_discord_save = make_button(hwnd, ID_DISCORD_SAVE, L"Save & connect");
    g_migrate_storage = make_button(hwnd, ID_MIGRATE_STORAGE, L"Migrate & restart");

    g_video_window = CreateWindowExW(0, VIDEO_CLASS, L"Charter Media Player - Video",
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

    BOOL editable_playlist = g_page == PAGE_PLAYLIST && g_active_playlist >= 0 &&
                             g_active_playlist < g_playlist_count &&
                             !g_playlists[g_active_playlist].is_builtin;
    int top_actions_w = g_page == PAGE_LIBRARY
        ? S(126 + 98 + 142) + gap * 3
        : (editable_playlist ? S(136 + 132) + gap * 2 : 0);
    int search_w = max(S(240), min(S(460), content_w - top_actions_w));
    MoveWindow(g_search_box, content_left, header_y, search_w, S(40), TRUE);

    int x = content_right;
    MoveWindow(g_open_library, x - S(126), header_y + S(1), S(126), action_h, TRUE); x -= S(126) + gap;
    MoveWindow(g_refresh, x - S(98), header_y + S(1), S(98), action_h, TRUE);
    x -= S(98) + gap;
    MoveWindow(g_import_media, x - S(142), header_y + S(1), S(142), action_h, TRUE);

    if (editable_playlist) {
        int playlist_x = content_right;
        MoveWindow(g_remove_playlist, playlist_x - S(136), header_y + S(1),
                   S(136), action_h, TRUE);
        playlist_x -= S(136) + gap;
        MoveWindow(g_edit_playlist, playlist_x - S(132), header_y + S(1),
                   S(132), action_h, TRUE);
    }

    int list_top = S(168);
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
    MoveWindow(g_output, content_left + S(24), S(202), output_w, S(220), TRUE);
    MoveWindow(g_refresh_outputs, content_left + S(24) + output_w + gap, S(202), S(148), S(38), TRUE);
    MoveWindow(g_migrate_storage, content_right - S(184), S(294), S(160), S(38), TRUE);

    int discord_inner_w = content_w - S(48);
    int discord_toggle_w = S(176);
    int discord_mode_w = S(116);
    int discord_save_w = S(136);
    int discord_x = content_left + S(24);
    MoveWindow(g_discord_mode, discord_x, S(478), discord_mode_w, S(38), TRUE);
    discord_x += discord_mode_w + gap;
    if (!g_discord_use_provided) {
        int discord_edit_w = max(S(160), discord_inner_w - discord_mode_w -
                                 discord_toggle_w - discord_save_w - gap * 3);
        MoveWindow(g_discord_app_id_edit, discord_x, S(483), discord_edit_w, S(28), TRUE);
        discord_x += discord_edit_w + gap;
    }
    MoveWindow(g_discord_toggle, discord_x, S(478), discord_toggle_w, S(38), TRUE);
    discord_x += discord_toggle_w + gap;
    MoveWindow(g_discord_save, discord_x, S(478), discord_save_w, S(38), TRUE);

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

    if (g_compact_mode) {
        RECT cover = { S(14), S(14), S(86), min(h - S(14), S(86)) };
        fill_round_rect(dc, cover, S(9), C_SURFACE);
        Track *compact_track = g_current_track_index < g_track_count ? &g_tracks[g_current_track_index] : NULL;
        if (compact_track && g_track_images && compact_track->image_index >= 0) {
            HICON icon = ImageList_GetIcon(g_track_images, compact_track->image_index, ILD_NORMAL);
            if (icon) {
                DrawIconEx(dc, cover.left + S(6), cover.top + S(6), icon,
                           S(60), S(60), 0, NULL, DI_NORMAL);
                DestroyIcon(icon);
            }
        } else if (g_icon_musical) {
            DrawIconEx(dc, cover.left + S(8), cover.top + S(8), g_icon_musical,
                       S(56), S(56), 0, NULL, DI_NORMAL);
        }
        RECT mini_title = { S(104), S(18), w - S(18), S(50) };
        draw_text_line(dc, g_playing_path[0] ? g_playing_title : L"Nothing playing",
                       mini_title, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT mini_artist = { S(104), S(50), w - S(18), S(76) };
        draw_text_line(dc, g_playing_path[0] ? g_playing_artist : L"Charter is in background mode",
                       mini_artist, g_font_body, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        return;
    }

    RECT side = { 0, 0, sidebar, player_top };
    HBRUSH sb = CreateSolidBrush(C_SIDEBAR);
    FillRect(dc, &side, sb);
    DeleteObject(sb);

    draw_app_mark(dc, S(16), S(18), S(34));
    RECT app_title = { S(58), S(16), sidebar - S(10), S(42) };
    draw_text_line(dc, L"Charter", app_title, g_font_body_semibold, C_TEXT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT app_sub = { S(58), S(39), sidebar - S(10), S(59) };
    draw_text_line(dc, L"Media Player", app_sub, g_font_small, C_TEXT_DIM,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    RECT pl_label = { S(18), S(238), sidebar - S(18), S(264) };
    draw_text_line(dc, L"PLAYLISTS", pl_label, g_font_small_semibold, C_TEXT_FAINT,
                   DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const wchar_t *page_title = L"Library";
    const wchar_t *page_sub = L"Everything saved in Charter Media Player";
    if (g_page == PAGE_DOWNLOADS) {
        page_title = L"Add music";
        page_sub = L"Download audio or MP4 video directly into your library";
    } else if (g_page == PAGE_PLAYLIST) {
        if (g_active_playlist >= 0 && g_active_playlist < g_playlist_count) page_title = g_playlists[g_active_playlist].name;
        else page_title = L"Playlist";
        page_sub = L"Your saved playlist";
    } else if (g_page == PAGE_SETTINGS) {
        page_title = L"Settings";
        page_sub = L"Playback, storage, and Discord presence";
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
        RECT count_rc = { content_left, S(135), content_left + S(180), S(162) };
        draw_text_line(dc, count_text, count_rc, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    if (g_page == PAGE_DOWNLOADS) {
        RECT card = { content_left, S(112), content_right, player_top - S(24) };
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

        wchar_t queue_title[96];
        swprintf(queue_title, ARRAY_LEN(queue_title), L"Download queue (%zu)", g_download_job_count);
        RECT queue_title_rc = { card.left + S(16), S(302), card.right - S(16), S(330) };
        draw_text_line(dc, queue_title, queue_title_rc, g_font_body_semibold, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int row_y = S(334);
        int row_h = S(58);
        int row_gap = S(7);
        int available = max(0, card.bottom - S(16) - row_y);
        size_t max_rows = (size_t)(available / max(1, row_h + row_gap));
        if (!g_download_job_count) {
            RECT empty = { card.left + S(16), row_y, card.right - S(16), row_y + row_h };
            fill_round_rect(dc, empty, S(8), C_SURFACE);
            RECT empty_text = empty;
            empty_text.left += S(14);
            draw_text_line(dc, L"No active downloads. Add a URL above to begin.", empty_text,
                           g_font_small, C_TEXT_DIM,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            size_t rows = min(g_download_job_count, max_rows);
            for (size_t i = 0; i < rows; ++i) {
                DownloadJob *job = g_download_jobs[i];
                if (!job) continue;
                RECT item = { card.left + S(16), row_y, card.right - S(16), row_y + row_h };
                fill_round_rect(dc, item, S(8), C_SURFACE);

                wchar_t number[24];
                swprintf(number, ARRAY_LEN(number), L"#%u", job->id);
                RECT number_rc = { item.left + S(12), item.top + S(7), item.left + S(54), item.top + S(28) };
                draw_text_line(dc, number, number_rc, g_font_small_semibold, C_ACCENT,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                RECT url_rc = { item.left + S(56), item.top + S(6), item.right - S(72), item.top + S(29) };
                draw_text_line(dc, job->url, url_rc, g_font_small_semibold, C_TEXT,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                wchar_t detail[352];
                swprintf(detail, ARRAY_LEN(detail), L"%ls  ·  %ls", job->format, job->status);
                RECT detail_rc = { item.left + S(56), item.top + S(27), item.right - S(72), item.top + S(48) };
                draw_text_line(dc, detail, detail_rc, g_font_small, C_TEXT_DIM,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                wchar_t percent[24];
                swprintf(percent, ARRAY_LEN(percent), L"%d%%", min(100, max(0, job->percent)));
                RECT percent_rc = { item.right - S(66), item.top + S(7), item.right - S(12), item.top + S(29) };
                draw_text_line(dc, percent, percent_rc, g_font_small_semibold, C_TEXT,
                               DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                RECT progress_bg = { item.left + S(56), item.bottom - S(8), item.right - S(12), item.bottom - S(4) };
                fill_round_rect(dc, progress_bg, S(2), C_CARD_ALT);
                RECT progress = progress_bg;
                progress.right = progress.left + MulDiv(progress_bg.right - progress_bg.left,
                                                        min(100, max(0, job->percent)), 100);
                if (progress.right > progress.left) fill_round_rect(dc, progress, S(2), C_ACCENT);
                row_y += row_h + row_gap;
            }
            if (rows < g_download_job_count) {
                wchar_t more[64];
                swprintf(more, ARRAY_LEN(more), L"+ %zu more active download%ls",
                         g_download_job_count - rows,
                         g_download_job_count - rows == 1 ? L"" : L"s");
                RECT more_rc = { card.left + S(20), row_y, card.right - S(20), card.bottom - S(8) };
                draw_text_line(dc, more, more_rc, g_font_small, C_TEXT_DIM,
                               DT_LEFT | DT_TOP | DT_SINGLELINE);
            }
        }
    }

    if (g_page == PAGE_SETTINGS) {
        RECT card = { content_left, S(112), content_right, min(player_top - S(24), S(250)) };
        fill_round_rect(dc, card, S(12), C_CARD);
        stroke_round_rect(dc, card, S(12), C_BORDER_SOFT);
        RECT heading = { card.left + S(20), card.top + S(16), card.right - S(20), card.top + S(46) };
        draw_text_line(dc, L"Audio output", heading, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT info = { card.left + S(20), card.top + S(40), card.right - S(20), card.top + S(66) };
        draw_text_line(dc, L"Choose where Charter plays audio. Changes apply immediately to the current track.",
                       info, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT device_label = { card.left + S(24), card.top + S(66), card.left + S(270), card.top + S(90) };
        draw_text_line(dc, L"Output device", device_label, g_font_small_semibold, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT migration_card = { content_left, S(266), content_right,
                                min(player_top - S(24), S(360)) };
        fill_round_rect(dc, migration_card, S(12), C_CARD);
        stroke_round_rect(dc, migration_card, S(12), C_BORDER_SOFT);
        RECT migration_heading = { migration_card.left + S(20), migration_card.top + S(13),
                                   migration_card.right - S(20), migration_card.top + S(41) };
        draw_text_line(dc, L"Storage migrator", migration_heading, g_font_section, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT migration_info = { migration_card.left + S(20), migration_card.top + S(43),
                                migration_card.right - S(210), migration_card.bottom - S(14) };
        draw_text_line(dc, g_migration_status, migration_info, g_font_small, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_END_ELLIPSIS);

        RECT discord_card = { content_left, S(376), content_right,
                              player_top - S(24) };
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
        draw_text_line(dc, g_discord_use_provided ? L"Application profile" : L"Custom Discord Application ID",
                       app_id_label, g_font_small_semibold, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT discord_status = { discord_card.left + S(24), discord_card.top + S(137),
                                discord_card.right - S(24),
                                min(discord_card.bottom - S(4), discord_card.top + S(161)) };
        draw_text_line(dc, g_discord_status, discord_status, g_font_small,
                       g_discord_enabled ? C_ACCENT : C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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
    draw_text_line(dc, g_playing_path[0] ? g_playing_title : L"Nothing playing", np_title,
                   g_font_body_semibold, C_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT np_sub = { S(94), player_top + S(47), min(w / 2 - S(250), S(390)), player_top + S(72) };
    draw_text_line(dc, g_playing_path[0] ? g_playing_artist : (g_status_text[0] ? g_status_text : L"Choose a track from your library"),
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
        int row = (int)cd->nmcd.dwItemSpec;
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (!ListView_GetItem(g_list, &item) || (size_t)item.lParam >= g_track_count)
            return CDRF_DODEFAULT;
        Track *track = &g_tracks[(size_t)item.lParam];
        HDC dc = cd->nmcd.hdc;
        RECT bounds;
        ListView_GetItemRect(g_list, row, &bounds, LVIR_BOUNDS);
        BOOL selected = (ListView_GetItemState(g_list, row, LVIS_SELECTED) & LVIS_SELECTED) != 0;
        BOOL hot = row == g_list_hover_row;
        COLORREF background = selected ? blend_color(C_BG, C_ACCENT, 48)
                                       : (hot ? C_CARD_ALT : C_BG);
        if (track->starred && !selected && !hot)
            background = blend_color(C_BG, C_ACCENT, 13);
        HBRUSH brush = CreateSolidBrush(background);
        FillRect(dc, &bounds, brush);
        DeleteObject(brush);
        if (track->starred) {
            RECT marker = { bounds.left, bounds.top + S(5), bounds.left + S(4), bounds.bottom - S(5) };
            fill_round_rect(dc, marker, S(2), C_ACCENT);
        }
        if (g_list_dragging && row == g_list_drag_target)
            draw_line(dc, bounds.left, bounds.top, bounds.right, bounds.top, C_ACCENT, S(2));

        RECT icon_rc, title_rc, artist_rc, actions_rc;
        ListView_GetSubItemRect(g_list, row, 0, LVIR_BOUNDS, &icon_rc);
        ListView_GetSubItemRect(g_list, row, 1, LVIR_BOUNDS, &title_rc);
        ListView_GetSubItemRect(g_list, row, 2, LVIR_BOUNDS, &artist_rc);
        ListView_GetSubItemRect(g_list, row, 3, LVIR_BOUNDS, &actions_rc);
        if (g_track_images && track->image_index >= 0) {
            HICON cover = ImageList_GetIcon(g_track_images, track->image_index, ILD_NORMAL);
            if (cover) {
                int image_w = S(88);
                int image_h = S(52);
                DrawIconEx(dc, icon_rc.left + S(8),
                           icon_rc.top + (icon_rc.bottom - icon_rc.top - image_h) / 2,
                           cover, image_w, image_h, 0, NULL, DI_NORMAL);
                DestroyIcon(cover);
            }
        } else {
            HICON fallback = track->is_video && g_icon_youtube ? g_icon_youtube :
                             (g_icon_musical ? g_icon_musical : g_app_icon);
            if (fallback) {
                int size = S(34);
                DrawIconEx(dc, icon_rc.left + (icon_rc.right - icon_rc.left - size) / 2,
                           icon_rc.top + (icon_rc.bottom - icon_rc.top - size) / 2,
                           fallback, size, size, 0, NULL, DI_NORMAL);
            }
        }

        RECT title_line = title_rc;
        title_line.left += S(10);
        title_line.top += S(7);
        title_line.bottom = title_line.top + S(24);
        draw_text_line(dc, track->title, title_line, g_font_body_semibold, C_TEXT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        wchar_t size_text[64];
        human_size(track->size_bytes, size_text, ARRAY_LEN(size_text));
        RECT size_line = title_rc;
        size_line.left += S(10);
        size_line.top += S(31);
        size_line.bottom -= S(5);
        draw_text_line(dc, size_text, size_line, g_font_small, C_TEXT_FAINT,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        artist_rc.left += S(10);
        artist_rc.right -= S(8);
        draw_text_line(dc, track->artist, artist_rc, g_font_body, C_TEXT_DIM,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        BOOL playlist_actions = g_page == PAGE_PLAYLIST;
        HICON library_icons[] = { g_icon_star, g_icon_heart, g_icon_add, g_icon_folder, g_icon_trash };
        HICON playlist_icons[] = { g_icon_star, g_icon_heart, g_icon_folder, g_icon_close };
        HICON *action_icons = playlist_actions ? playlist_icons : library_icons;
        int action_count = playlist_actions ? 4 : 5;
        for (int i = 0; i < action_count; ++i) {
            RECT action = { actions_rc.left + S(4 + i * 40), actions_rc.top + S(10),
                            actions_rc.left + S(36 + i * 40), actions_rc.bottom - S(10) };
            if ((i == 0 && track->starred) || (i == 1 && track->liked))
                fill_round_rect(dc, action, S(7), blend_color(C_SURFACE, C_ACCENT, i == 1 ? 72 : 30));
            if (action_icons[i]) {
                int icon_size = S(20);
                DrawIconEx(dc, action.left + (action.right - action.left - icon_size) / 2,
                           action.top + (action.bottom - action.top - icon_size) / 2,
                           action_icons[i], icon_size, icon_size, 0, NULL, DI_NORMAL);
            }
        }
        draw_line(dc, bounds.left, bounds.bottom - 1, bounds.right, bounds.bottom - 1,
                  C_BORDER_SOFT, 1);
        return CDRF_SKIPDEFAULT;
    }

    return CDRF_DODEFAULT;
}

static void update_button_enabled_state(void) {
    BOOL has_selection = selected_track() != NULL;
    BOOL downloading = InterlockedCompareExchange(&g_downloading, 0, 0) != 0;
    BOOL installing = InterlockedCompareExchange(&g_installing_tools, 0, 0) != 0;

    refresh_tool_paths();

    if (g_download) {
        SetWindowTextW(g_download, installing ? L"Preparing..." : L"Download");
    }

    if (g_play) EnableWindow(g_play, has_selection || g_current_track_index < g_track_count);
    if (g_previous) EnableWindow(g_previous, ListView_GetItemCount(g_list) > 0);
    if (g_next) EnableWindow(g_next, ListView_GetItemCount(g_list) > 0);
    if (g_open_folder) EnableWindow(g_open_folder, has_selection);
    if (g_add_playlist) EnableWindow(g_add_playlist, has_selection);
    if (g_delete_media) EnableWindow(g_delete_media, has_selection);
    if (g_star_media) EnableWindow(g_star_media, has_selection);
    if (g_heart_media) EnableWindow(g_heart_media, has_selection);
    if (g_remove_playlist) EnableWindow(g_remove_playlist,
        g_page == PAGE_PLAYLIST && g_active_playlist >= 0 &&
        g_active_playlist < g_playlist_count && !g_playlists[g_active_playlist].is_builtin);
    if (g_edit_playlist) EnableWindow(g_edit_playlist, g_active_playlist >= 0 &&
        g_page == PAGE_PLAYLIST && !g_playlists[g_active_playlist].is_builtin);
    if (g_open_library) EnableWindow(g_open_library, g_library_ready);
    if (g_refresh) EnableWindow(g_refresh, g_library_ready);
    if (g_import_media) EnableWindow(g_import_media, g_library_ready);

    if (g_url_edit) EnableWindow(g_url_edit, !installing);
    if (g_format) EnableWindow(g_format, !installing);
    if (g_resolution) EnableWindow(g_resolution, !installing);
    if (g_download) EnableWindow(g_download, g_library_ready && !installing);
    if (g_install_tools) EnableWindow(g_install_tools, !downloading && !installing);
    if (g_output) EnableWindow(g_output, TRUE);
    if (g_discord_app_id_edit) {
        EnableWindow(g_discord_app_id_edit, TRUE);
        SendMessageW(g_discord_app_id_edit, EM_SETREADONLY, g_discord_use_provided, 0);
    }
    if (g_discord_mode) EnableWindow(g_discord_mode, TRUE);
    if (g_discord_toggle) EnableWindow(g_discord_toggle, TRUE);
    if (g_discord_save) EnableWindow(g_discord_save, TRUE);
    if (g_migrate_storage) {
        EnableWindow(g_migrate_storage, g_using_legacy_storage);
        SetWindowTextW(g_migrate_storage,
                       g_using_legacy_storage ? L"Migrate & restart" : L"No migration needed");
    }

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
    if (g_discord_use_provided) {
        wcscpy(app_id, DISCORD_PROVIDED_APP_ID);
    } else {
        discord_read_app_id_field(app_id, ARRAY_LEN(app_id));
        wcsncpy(g_discord_custom_app_id, app_id, ARRAY_LEN(g_discord_custom_app_id) - 1);
        g_discord_custom_app_id[ARRAY_LEN(g_discord_custom_app_id) - 1] = L'\0';
    }
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

static void toggle_discord_id_mode(void) {
    if (!g_discord_use_provided) {
        discord_read_app_id_field(g_discord_custom_app_id, ARRAY_LEN(g_discord_custom_app_id));
    }
    discord_disconnect(TRUE);
    g_discord_use_provided = !g_discord_use_provided;
    if (g_discord_use_provided) wcscpy(g_discord_app_id, DISCORD_PROVIDED_APP_ID);
    else wcsncpy(g_discord_app_id, g_discord_custom_app_id, ARRAY_LEN(g_discord_app_id) - 1);
    g_discord_app_id[ARRAY_LEN(g_discord_app_id) - 1] = L'\0';
    update_discord_mode_ui();
    discord_set_status(g_discord_use_provided
        ? L"Using Charter's provided Discord application. Save to connect."
        : L"Enter your custom Discord Application ID, then save.");
    save_discord_config();
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
        if (g_discord_use_provided) wcscpy(app_id, DISCORD_PROVIDED_APP_ID);
        else discord_read_app_id_field(app_id, ARRAY_LEN(app_id));
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

static void request_storage_migration(void) {
    if (!g_using_legacy_storage || !g_legacy_storage_available) {
        set_status(L"No automatic legacy migration is currently available.");
        return;
    }
    int answer = MessageBoxW(
        g_main,
        L"Charter Media Player will close, move all Charter Music Browser data "
        L"to the new AppData folder, update playlist paths, and restart.\n\n"
        L"Any active downloads will be cancelled. Continue?",
        L"Migrate legacy storage",
        MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2);
    if (answer != IDYES) return;
    g_storage_migration_requested = TRUE;
    SendMessageW(g_main, WM_CLOSE, 0, 0);
}

static BOOL CALLBACK set_child_visibility(HWND child, LPARAM show) {
    ShowWindow(child, show ? SW_SHOW : SW_HIDE);
    return TRUE;
}

static void enter_compact_mode(void) {
    if (!g_compact_background_enabled || g_compact_mode || !g_main) return;
    GetWindowRect(g_main, &g_restore_window_rect);
    g_compact_mode = TRUE;
    EnumChildWindows(g_main, set_child_visibility, FALSE);
    int width = S(560), height = S(132);
    SetWindowPos(g_main, NULL, g_restore_window_rect.left, g_restore_window_rect.top,
                 width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(g_main, NULL, FALSE);
}

static void leave_compact_mode(void) {
    if (!g_compact_mode || !g_main) return;
    g_compact_mode = FALSE;
    SetWindowPos(g_main, NULL, g_restore_window_rect.left, g_restore_window_rect.top,
                 g_restore_window_rect.right - g_restore_window_rect.left,
                 g_restore_window_rect.bottom - g_restore_window_rect.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    EnumChildWindows(g_main, set_child_visibility, TRUE);
    if (g_install_tools) ShowWindow(g_install_tools, SW_HIDE);
    update_page_visibility();
    layout_ui(g_main);
}

static void select_only_list_row(int row) {
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
}

static void handle_row_action(const NMLISTVIEW *click) {
    if (!click || click->iItem < 0 || click->iSubItem != 3) return;
    LVITEMW item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = click->iItem;
    if (!ListView_GetItem(g_list, &item) || (size_t)item.lParam >= g_track_count) return;
    RECT actions;
    ListView_GetSubItemRect(g_list, click->iItem, 3, LVIR_BOUNDS, &actions);
    int relative_x = click->ptAction.x - actions.left - S(4);
    int action_count = g_page == PAGE_PLAYLIST ? 4 : 5;
    if (relative_x < 0 || relative_x >= S(40) * action_count) return;
    int slot = relative_x / max(1, S(40));
    size_t index = (size_t)item.lParam;
    if (slot == 0) {
        set_track_starred(index, !g_tracks[index].starred);
        save_media_flags();
        populate_list();
    } else if (slot == 1) {
        set_track_liked(index, !g_tracks[index].liked);
        save_media_flags();
        sync_liked_playlist();
        if (g_page == PAGE_PLAYLIST && g_active_playlist >= 0 &&
            g_active_playlist < g_playlist_count &&
            g_playlists[g_active_playlist].is_builtin) load_active_playlist();
        populate_list();
    } else if (g_page == PAGE_PLAYLIST && slot == 2) {
        wchar_t args[MAX_PATH * 4 + 32];
        swprintf(args, ARRAY_LEN(args), L"/select,\"%ls\"", g_tracks[index].path);
        ShellExecuteW(g_main, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
    } else if (g_page == PAGE_PLAYLIST && slot == 3) {
        select_only_list_row(click->iItem);
        remove_selected_from_active_playlist();
    } else if (g_page == PAGE_LIBRARY && slot == 2) {
        select_only_list_row(click->iItem);
        show_add_to_playlist_menu();
    } else if (g_page == PAGE_LIBRARY && slot == 3) {
        wchar_t args[MAX_PATH * 4 + 32];
        swprintf(args, ARRAY_LEN(args), L"/select,\"%ls\"", g_tracks[index].path);
        ShellExecuteW(g_main, L"open", L"explorer.exe", args, NULL, SW_SHOWNORMAL);
    } else if (g_page == PAGE_LIBRARY && slot == 4) {
        select_only_list_row(click->iItem);
        delete_selected_media();
    }
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
            if (wParam == SIZE_MINIMIZED) return 0;
            if (g_compact_mode) { InvalidateRect(hwnd, NULL, FALSE); return 0; }
            layout_ui(hwnd);
            return 0;

        case WM_ACTIVATEAPP:
            if (g_compact_background_enabled) {
                if (wParam) leave_compact_mode();
                else enter_compact_mode();
            }
            return 0;

        case WM_SYSCOMMAND:
            if (g_compact_background_enabled && (wParam & 0xFFF0) == SC_MINIMIZE) {
                enter_compact_mode();
                return 0;
            }
            break;

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
                if (delete_active_playlist()) {
                    set_page(PAGE_LIBRARY);
                    set_status(L"Playlist deleted. Its media remains in the Library.");
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
                mark_player_preferences_dirty();
                set_status(g_loop_enabled ? L"Loop is on." : L"Loop is off.");
                update_button_enabled_state();
            } else if (id == ID_SHUFFLE && code == BN_CLICKED) {
                g_shuffle_enabled = !g_shuffle_enabled;
                mark_player_preferences_dirty();
                set_status(g_shuffle_enabled ? L"Shuffle is on." : L"Shuffle is off.");
                update_button_enabled_state();
            } else if (id == ID_OPEN_FOLDER && code == BN_CLICKED) {
                open_selected_folder();
            } else if (id == ID_REFRESH_OUTPUTS && code == BN_CLICKED) {
                enumerate_audio_outputs();
                set_status(L"Audio output devices refreshed.");
            } else if (id == ID_DISCORD_TOGGLE && code == BN_CLICKED) {
                toggle_discord_presence();
            } else if (id == ID_DISCORD_MODE && code == BN_CLICKED) {
                toggle_discord_id_mode();
            } else if (id == ID_DISCORD_SAVE && code == BN_CLICKED) {
                save_discord_settings_from_ui();
            } else if (id == ID_MIGRATE_STORAGE && code == BN_CLICKED) {
                request_storage_migration();
            } else if (id == ID_OUTPUT &&
                       (code == CBN_SELCHANGE || code == CBN_SELENDOK)) {
                int sel = (int)SendMessageW(g_output, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < g_audio_output_count) {
                    BOOL output_changed = sel != g_audio_output_index;
                    BOOL preference_changed = _wcsicmp(
                        g_audio_output_preference, g_audio_outputs[sel].name) != 0;
                    g_audio_output_index = sel;
                    if (preference_changed) {
                        wcsncpy(g_audio_output_preference, g_audio_outputs[sel].name,
                                ARRAY_LEN(g_audio_output_preference) - 1);
                        g_audio_output_preference[
                            ARRAY_LEN(g_audio_output_preference) - 1] = L'\0';
                        mark_player_preferences_dirty();
                    }
                    if (output_changed) restart_on_selected_output();
                    if (output_changed || preference_changed)
                        set_status(L"Audio output changed.");
                }
            }
            return 0;
        }

        case WM_APP_SLIDER_CHANGED:
            if ((int)wParam == ID_VOLUME) {
                g_volume_percent = (int)lParam;
                mark_player_preferences_dirty();
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

        case WM_APP_VIDEO_AUDIO_READY: {
            LONG generation = (LONG)lParam;
            if (generation != InterlockedCompareExchange(
                                  &g_video_audio_generation, 0, 0) ||
                !g_video_player)
                return 0;
            BOOL failed = wParam != 0;
            g_video_audio_ready = TRUE;
            g_video_audio_failed = failed;
            if (failed && g_video_start_dispatched) {
                g_video_uses_custom_audio = FALSE;
                apply_player_volume();
            } else {
                start_video_when_ready();
            }
            if (failed)
                set_status(L"Video is playing through the Windows default output because the selected device could not decode this audio track.");
            return 0;
        }

        case WM_APP_VIDEO_MEDIA_READY:
            if ((IMFPMediaPlayer *)lParam == g_video_player) {
                g_video_media_ready = TRUE;
                start_video_when_ready();
            }
            return 0;

        case WM_TIMER:
            if (wParam == 1) {
                if (g_player_preferences_dirty &&
                    GetTickCount64() - g_player_preferences_changed_at >= 750) {
                    if (!save_player_preferences())
                        g_player_preferences_changed_at = GetTickCount64();
                }
                if (!g_compact_mode && !g_seek_dragging && g_current_track_index < g_track_count) {
                    LONGLONG duration = player_duration_100ns();
                    LONGLONG pos = player_position_100ns();
                    if (duration > 0) {
                        int value = (int)((pos * 1000LL) / duration);
                        slider_set_value(g_seek, value);
                        if (g_video_seek) slider_set_value(g_video_seek, value);
                    }
                }
                if (!g_compact_mode) {
                    RECT player_rc;
                    GetClientRect(hwnd, &player_rc);
                    player_rc.top = max(0, player_rc.bottom - S(110));
                    InvalidateRect(hwnd, &player_rc, FALSE);
                }
                discord_tick();
            }
            return 0;

        case WM_APP_LIBRARY_BATCH: {
            LibraryBatchResult *batch = (LibraryBatchResult *)lParam;
            if (!batch) return 0;
            if (batch->generation != current_library_generation()) {
                free_library_batch_result(batch);
                return 0;
            }

            for (size_t b = 0; b < batch->count; ++b) {
                LibraryItemResult *result = &batch->items[b];
                size_t track_index = (size_t)-1;
                for (size_t i = 0; i < g_track_count; ++i) {
                    if (_wcsicmp(g_tracks[i].path, result->path) == 0 &&
                        g_tracks[i].size_bytes == result->size_bytes &&
                        g_tracks[i].last_write_time == result->last_write_time) {
                        track_index = i;
                        break;
                    }
                }

                if (track_index < g_track_count) {
                    Track *track = &g_tracks[track_index];
                    wchar_t title_buf[MAX_PATH * 2];
                    title_from_filename(base_name(track->path), title_buf,
                                        ARRAY_LEN(title_buf));
                    const wchar_t *title = result->title && result->title[0]
                                               ? result->title : title_buf;
                    const wchar_t *artist = result->artist && result->artist[0]
                                                ? result->artist : L"Unknown artist";
                    wchar_t *new_title = dup_wstr(title);
                    wchar_t *new_artist = dup_wstr(artist);
                    if (new_title && new_artist) {
                        free(track->title);
                        free(track->artist);
                        track->title = new_title;
                        track->artist = new_artist;
                    } else {
                        free(new_title);
                        free(new_artist);
                    }

                    BOOL cached = store_media_details_cache(result);
                    if (cached) result->thumbnail = NULL;
                    MediaDetailsCacheEntry *cache_entry =
                        find_media_details_cache(track->path);
                    HBITMAP thumbnail = cached && cache_entry
                                            ? cache_entry->thumbnail
                                            : result->thumbnail;
                    if (track->image_index < 0 && thumbnail && g_track_images)
                        track->image_index = ImageList_Add(
                            g_track_images, thumbnail, NULL);

                    int rows = ListView_GetItemCount(g_list);
                    for (int row = 0; row < rows; ++row) {
                        LVITEMW item;
                        ZeroMemory(&item, sizeof(item));
                        item.mask = LVIF_PARAM;
                        item.iItem = row;
                        if (!ListView_GetItem(g_list, &item) ||
                            (size_t)item.lParam != track_index)
                            continue;
                        ListView_SetItemText(g_list, row, 1, track->title);
                        ListView_SetItemText(g_list, row, 2, track->artist);
                        item.mask = LVIF_IMAGE;
                        item.iImage = track->image_index;
                        ListView_SetItem(g_list, &item);
                        break;
                    }
                }
                ++g_library_details_loaded;
            }

            if (g_library_details_loaded > g_library_details_total)
                g_library_details_loaded = g_library_details_total;
            wchar_t status[256];
            swprintf(status, ARRAY_LEN(status),
                     L"Library ready. Loading details... %zu of %zu.",
                     g_library_details_loaded, g_library_details_total);
            set_status(status);
            InvalidateRect(g_list, NULL, FALSE);
            free_library_batch_result(batch);
            return 0;
        }

        case WM_APP_LIBRARY_DONE: {
            if ((LONG)wParam != current_library_generation()) return 0;
            g_library_details_loaded = g_library_details_total;
            wchar_t query[512] = L"";
            if (g_search) GetWindowTextW(g_search, query, ARRAY_LEN(query));
            if (query[0]) populate_list();
            wchar_t status[256];
            swprintf(status, ARRAY_LEN(status),
                     L"Library ready. %zu media item%ls found.",
                     g_track_count, g_track_count == 1 ? L"" : L"s");
            set_status(status);
            update_button_enabled_state();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        case WM_APP_DOWNLOAD_UPDATE: {
            DownloadUpdate *update = (DownloadUpdate *)lParam;
            if (update) {
                int pct = update->percent;
                if (pct < 0 && InterlockedCompareExchange(&g_downloading, 0, 0)) pct = g_download_percent;
                DownloadJob *updated_job = (DownloadJob *)update->job;
                if (updated_job) {
                    for (size_t i = 0; i < g_download_job_count; ++i) {
                        if (g_download_jobs[i] != updated_job) continue;
                        wcsncpy(updated_job->status, update->text,
                                ARRAY_LEN(updated_job->status) - 1);
                        updated_job->status[ARRAY_LEN(updated_job->status) - 1] = L'\0';
                        if (update->percent >= 0) updated_job->percent = update->percent;
                        break;
                    }
                }
                set_download_status(update->text, pct);
                free(update);
            }
            return 0;
        }

        case WM_APP_DOWNLOAD_DONE: {
            DWORD exit_code = (DWORD)wParam;
            DownloadJob *job = (DownloadJob *)lParam;
            wchar_t error[1024] = L"";
            unsigned job_id = job ? job->id : 0;
            if (job) {
                wcsncpy(error, job->last_error, ARRAY_LEN(error) - 1);
                if (job->process) CloseHandle(job->process);
                if (job->thread) CloseHandle(job->thread);
                forget_download_job(job);
                free(job->command);
                free(job);
            }
            LONG remaining = InterlockedDecrement(&g_downloading);
            if (remaining < 0) { InterlockedExchange(&g_downloading, 0); remaining = 0; }

            if (exit_code == 0) {
                wchar_t done[256];
                swprintf(done, ARRAY_LEN(done),
                         L"Download #%u finished. %ld download%ls still active.",
                         job_id, remaining, remaining == 1 ? L"" : L"s");
                set_download_status(done, 100);
                refresh_library();
            } else if (exit_code == 2) {
                set_download_status(L"Download cancelled.", -1);
            } else if (error[0]) {
                set_download_status(error, -1);
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
                    NMLISTVIEW *click = (NMLISTVIEW *)lParam;
                    if (click->iItem >= 0 && click->iSubItem != 3) {
                        play_selected();
                        update_button_enabled_state();
                    }
                    return 0;
                }
                if (hdr->code == LVN_ITEMCHANGED) {
                    InvalidateRect(g_list, NULL, FALSE);
                    update_button_enabled_state();
                    return 0;
                }
                if (hdr->code == LVN_BEGINDRAG) {
                    NMLISTVIEW *drag = (NMLISTVIEW *)lParam;
                    g_list_dragging = TRUE;
                    g_list_drag_start = drag->iItem;
                    g_list_drag_target = drag->iItem;
                    SetCapture(g_list);
                    return 0;
                }
                if (hdr->code == NM_CLICK) {
                    NMLISTVIEW *click = (NMLISTVIEW *)lParam;
                    if (click->iItem >= 0 && click->iSubItem == 3) {
                        handle_row_action(click);
                    }
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

        case WM_CTLCOLORSTATIC:
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
                if (g_page == PAGE_PLAYLIST) remove_selected_from_active_playlist();
                else if (g_page == PAGE_LIBRARY) delete_selected_media();
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
            mmi->ptMinTrackSize.x = g_compact_mode ? S(420) : S(1020);
            mmi->ptMinTrackSize.y = g_compact_mode ? S(118) : S(700);
            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (g_player_preferences_dirty) save_player_preferences();
            InterlockedIncrement(&g_library_load_generation);
            discord_disconnect(TRUE);
            stop_playback();
            if (g_video_window) {
                DestroyWindow(g_video_window);
                g_video_window = NULL;
            }
            free_audio_outputs();
            free_playlist_tracks();
            shutdown_download_jobs();
            if (g_tools_thread) { CloseHandle(g_tools_thread); g_tools_thread = NULL; }
            free_tracks();
            free_media_details_cache();
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
