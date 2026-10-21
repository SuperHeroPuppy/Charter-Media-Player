/* Persistent, user-editable palettes. The built-in Charter palette is immutable. */

typedef struct ThemeEntry {
    wchar_t name[64];
    wchar_t file_name[80];
    int kind; /* 0: immutable built-in, 3: user-created or imported */
} ThemeEntry;

static const COLORREF g_default_palette[TC_COUNT] = {
    RGB(32,32,32), RGB(43,43,43), RGB(47,47,47), RGB(50,50,50),
    RGB(58,58,58), RGB(64,64,64), RGB(70,70,70), RGB(58,58,58),
    RGB(245,245,245), RGB(174,174,174), RGB(132,132,132),
    RGB(239,159,9), RGB(247,174,37), RGB(220,143,0), RGB(28,22,12),
    RGB(57,57,57), RGB(38,38,38), RGB(24,24,24), RGB(26,26,26),
    RGB(52,52,52), RGB(126,38,45), RGB(79,31,35)
};

static const COLORREF g_light_palette[TC_COUNT] = {
    RGB(246,247,249), RGB(255,255,255), RGB(240,242,245), RGB(241,243,246),
    RGB(231,234,239), RGB(219,224,231), RGB(199,204,212), RGB(223,227,233),
    RGB(27,31,36), RGB(84,91,101), RGB(119,126,137),
    RGB(170,103,0), RGB(194,122,10), RGB(143,83,0), RGB(255,255,255),
    RGB(219,230,246), RGB(232,235,239), RGB(234,237,242), RGB(246,247,249),
    RGB(205,210,216), RGB(247,217,220), RGB(238,193,199)
};

static const wchar_t *g_theme_color_keys[TC_COUNT] = {
    L"background", L"card", L"cardAlt", L"surface", L"surfaceHover",
    L"surfacePress", L"border", L"borderSoft", L"text", L"textDim",
    L"textFaint", L"accent", L"accentHover", L"accentPress",
    L"accentText", L"selected", L"header", L"sidebar", L"player",
    L"playerTop", L"blacklist", L"blacklistDark"
};

static const wchar_t *g_theme_color_labels[TC_COUNT] = {
    L"Background", L"Cards", L"Raised cards", L"Controls", L"Control hover",
    L"Control press", L"Borders", L"Soft borders", L"Text", L"Muted text",
    L"Faint text", L"Accent", L"Accent hover", L"Accent press",
    L"Accent text", L"Selection", L"Headers", L"Sidebar", L"Player bar",
    L"Player divider", L"Blacklist", L"Blacklist dark"
};

_Static_assert(ARRAY_LEN(g_default_palette) == TC_COUNT, "default theme color count");
_Static_assert(ARRAY_LEN(g_light_palette) == TC_COUNT, "light theme color count");
_Static_assert(ARRAY_LEN(g_theme_color_keys) == TC_COUNT, "theme JSON color count");
_Static_assert(ARRAY_LEN(g_theme_color_labels) == TC_COUNT, "theme builder color count");

static ThemeEntry g_theme_entries[64];
static int g_theme_entry_count;
static int g_active_theme_index;
static BOOL g_theme_selector_updating;
static BOOL g_theme_builder_open;
static int g_theme_builder_scroll;
static COLORREF g_theme_dialog_colors[16];
static HANDLE g_theme_change_handle = INVALID_HANDLE_VALUE;
static BOOL g_theme_refresh_pending;

static void theme_set_base_palette(BOOL light) {
    memcpy(g_theme_colors, light ? g_light_palette : g_default_palette,
           sizeof(g_theme_colors));
    wcscpy(g_theme_body_font, L"Segoe UI Variable Text");
    wcscpy(g_theme_display_font, L"Segoe UI Variable Display");
    g_theme_is_light = light;
}

static void initialize_theme_palette(void) {
    theme_set_base_palette(FALSE);
}

static void theme_file_path(const wchar_t *name, wchar_t *out, size_t capacity) {
    swprintf(out, capacity, L"%ls\\%ls", g_themes_root, name);
}

static BOOL theme_filename_for_name(const wchar_t *name, const wchar_t *current,
                                    wchar_t *result, size_t capacity) {
    wchar_t slug[49];
    size_t used = 0;
    for (const wchar_t *at = name; *at && used + 1 < ARRAY_LEN(slug); ++at) {
        wchar_t ch = *at;
        BOOL allowed = (iswalnum(ch) || ch >= 128) &&
                       !wcschr(L"<>:\"/\\|?*", ch) && !iswcntrl(ch);
        if (allowed) slug[used++] = towlower(ch);
        else if (used && slug[used - 1] != L'-') slug[used++] = L'-';
    }
    while (used && slug[used - 1] == L'-') --used;
    slug[used] = L'\0';
    if (!used) wcscpy(slug, L"untitled");
    for (unsigned suffix = 1; suffix <= 999; ++suffix) {
        swprintf(result, capacity, suffix == 1 ? L"theme-%ls.json" :
                 L"theme-%ls-%u.json", slug, suffix);
        if (current && _wcsicmp(result, current) == 0) return TRUE;
        wchar_t path[MAX_PATH * 4];
        theme_file_path(result, path, ARRAY_LEN(path));
        if (!file_exists(path)) return TRUE;
    }
    return FALSE;
}

static BOOL write_utf8_theme_file(const wchar_t *path, const wchar_t *text_value) {
    int bytes = WideCharToMultiByte(CP_UTF8, 0, text_value, -1, NULL, 0, NULL, NULL);
    if (bytes <= 1 || bytes > 65536) return FALSE;
    char *utf8 = (char *)malloc((size_t)bytes);
    if (!utf8) return FALSE;
    if (!WideCharToMultiByte(CP_UTF8, 0, text_value, -1, utf8, bytes, NULL, NULL)) {
        free(utf8);
        return FALSE;
    }
    wchar_t temp[MAX_PATH * 4];
    swprintf(temp, ARRAY_LEN(temp), L"%ls.tmp", path);
    HANDLE file = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { free(utf8); return FALSE; }
    DWORD written = 0;
    BOOL ok = WriteFile(file, utf8, (DWORD)(bytes - 1), &written, NULL) &&
              written == (DWORD)(bytes - 1) && FlushFileBuffers(file);
    CloseHandle(file);
    free(utf8);
    if (!ok || !MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp);
        return FALSE;
    }
    return TRUE;
}

static wchar_t *read_utf8_theme_file(const wchar_t *path) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size > 65536) { CloseHandle(file); return NULL; }
    char *bytes = (char *)malloc((size_t)size + 1);
    if (!bytes) { CloseHandle(file); return NULL; }
    DWORD read = 0;
    BOOL ok = ReadFile(file, bytes, size, &read, NULL);
    CloseHandle(file);
    if (!ok || read != size) { free(bytes); return NULL; }
    bytes[size] = '\0';
    int offset = size >= 3 && (unsigned char)bytes[0] == 0xEF &&
                 (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF ? 3 : 0;
    int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes + offset,
                                    (int)(size - offset), NULL, 0);
    if (chars <= 0) { free(bytes); return NULL; }
    wchar_t *wide = (wchar_t *)calloc((size_t)chars + 1, sizeof(wchar_t));
    if (wide) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes + offset,
                                 (int)(size - offset), wide, chars);
    free(bytes);
    return wide;
}

static BOOL json_scan_string(const wchar_t **cursor, wchar_t *out,
                             size_t capacity, BOOL *truncated) {
    const wchar_t *at = *cursor;
    if (*at++ != L'"') return FALSE;
    size_t used = 0;
    while (*at && *at != L'"') {
        wchar_t c = *at++;
        if (c == L'\\') {
            c = *at++;
            if (c == L'n' || c == L'r' || c == L't') c = L' ';
            else if (c != L'"' && c != L'\\') return FALSE;
        }
        if (used + 1 < capacity) out[used++] = c;
        else *truncated = TRUE;
    }
    if (*at != L'"') return FALSE;
    *cursor = at + 1;
    out[used] = L'\0';
    return TRUE;
}

static BOOL json_theme_string(const wchar_t *json, const wchar_t *key,
                              wchar_t *out, size_t capacity) {
    if (!json || !capacity) return FALSE;
    const wchar_t *at = json;
    while (*at) {
        if (*at != L'"') { ++at; continue; }
        wchar_t token[80];
        BOOL truncated = FALSE;
        if (!json_scan_string(&at, token, ARRAY_LEN(token), &truncated))
            return FALSE;
        const wchar_t *next = at;
        while (iswspace(*next)) ++next;
        if (truncated || *next != L':' || wcscmp(token, key) != 0) continue;
        at = next + 1;
        while (iswspace(*at)) ++at;
        truncated = FALSE;
        return json_scan_string(&at, out, capacity, &truncated) && !truncated;
    }
    return FALSE;
}

static void append_theme_json_string(wchar_t *output, size_t capacity,
                                     size_t *used, const wchar_t *value) {
    for (const wchar_t *at = value; *at && *used + 2 < capacity; ++at) {
        if (*at == L'"' || *at == L'\\') output[(*used)++] = L'\\';
        output[(*used)++] = iswcntrl(*at) ? L' ' : *at;
    }
    output[*used] = L'\0';
}

static BOOL theme_read_entry(const ThemeEntry *entry) {
    if (entry->kind == 0) { theme_set_base_palette(FALSE); return TRUE; }
    theme_set_base_palette(FALSE);
    wchar_t path[MAX_PATH * 4];
    theme_file_path(entry->file_name, path, ARRAY_LEN(path));
    wchar_t *json = read_utf8_theme_file(path);
    if (!json) return FALSE;
    wchar_t base[32];
    if (json_theme_string(json, L"base", base, ARRAY_LEN(base)) &&
        _wcsicmp(base, L"light") == 0) theme_set_base_palette(TRUE);
    json_theme_string(json, L"bodyFont", g_theme_body_font,
                      ARRAY_LEN(g_theme_body_font));
    json_theme_string(json, L"displayFont", g_theme_display_font,
                      ARRAY_LEN(g_theme_display_font));
    for (int color = 0; color < TC_COUNT; ++color) {
        wchar_t value[16];
        if (!json_theme_string(json, g_theme_color_keys[color], value,
                               ARRAY_LEN(value)) || wcslen(value) != 7 ||
            value[0] != L'#') continue;
        wchar_t *end = NULL;
        unsigned long rgb = wcstoul(value + 1, &end, 16);
        if (!end || *end || rgb > 0xFFFFFFUL) continue;
        g_theme_colors[color] = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    }
    free(json);
    return TRUE;
}

static BOOL theme_save_entry_to_path(const ThemeEntry *entry, const wchar_t *path) {
    if (!entry || !path) return FALSE;
    wchar_t json[8192];
    size_t used = 0;
    used += swprintf(json + used, ARRAY_LEN(json) - used,
                     L"{\n  \"schema\": 1,\n  \"name\": \"");
    append_theme_json_string(json, ARRAY_LEN(json), &used, entry->name);
    used += swprintf(json + used, ARRAY_LEN(json) - used,
                     L"\",\n  \"base\": \"%ls\",\n  \"bodyFont\": \"",
                     g_theme_is_light ? L"light" : L"dark");
    append_theme_json_string(json, ARRAY_LEN(json), &used, g_theme_body_font);
    used += swprintf(json + used, ARRAY_LEN(json) - used,
                     L"\",\n  \"displayFont\": \"");
    append_theme_json_string(json, ARRAY_LEN(json), &used, g_theme_display_font);
    used += swprintf(json + used, ARRAY_LEN(json) - used, L"\",\n  \"colors\": {\n");
    for (int color = 0; color < TC_COUNT; ++color) {
        COLORREF rgb = g_theme_colors[color];
        used += swprintf(json + used, ARRAY_LEN(json) - used,
                         L"    \"%ls\": \"#%02X%02X%02X\"%ls\n",
                         g_theme_color_keys[color], GetRValue(rgb), GetGValue(rgb),
                         GetBValue(rgb), color == TC_COUNT - 1 ? L"" : L",");
    }
    swprintf(json + used, ARRAY_LEN(json) - used, L"  }\n}\n");
    return write_utf8_theme_file(path, json);
}

static BOOL theme_save_entry(const ThemeEntry *entry) {
    if (!entry || entry->kind == 0) return FALSE;
    wchar_t path[MAX_PATH * 4];
    theme_file_path(entry->file_name, path, ARRAY_LEN(path));
    return theme_save_entry_to_path(entry, path);
}

static BOOL theme_save_selection(void) {
    if (!g_themes_root[0] || g_active_theme_index >= g_theme_entry_count)
        return FALSE;
    wchar_t path[MAX_PATH * 4], json[256];
    theme_file_path(L"active.json", path, ARRAY_LEN(path));
    swprintf(json, ARRAY_LEN(json), L"{\"active\":\"%ls\"}\n",
             g_active_theme_index == 0 ? L"default" :
             g_theme_entries[g_active_theme_index].file_name);
    return write_utf8_theme_file(path, json);
}

static void theme_refresh_selector(void) {
    if (!g_theme_select) return;
    g_theme_selector_updating = TRUE;
    SendMessageW(g_theme_select, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_theme_entry_count; ++i)
        SendMessageW(g_theme_select, CB_ADDSTRING, 0,
                     (LPARAM)g_theme_entries[i].name);
    SendMessageW(g_theme_select, CB_SETCURSEL, g_active_theme_index, 0);
    g_theme_selector_updating = FALSE;
}

static void load_themes(void) {
    if (!g_themes_root[0] || !create_directory_if_needed(g_themes_root)) return;
    if (g_theme_change_handle == INVALID_HANDLE_VALUE)
        g_theme_change_handle = FindFirstChangeNotificationW(
            g_themes_root, FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    g_theme_entry_count = 1;
    g_theme_entries[0] = (ThemeEntry){ L"Default Charter (locked)", L"", 0 };

    wchar_t pattern[MAX_PATH * 4];
    theme_file_path(L"*.json", pattern, ARRAY_LEN(pattern));
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            if (g_theme_entry_count >= (int)ARRAY_LEN(g_theme_entries)) break;
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (_wcsicmp(found.cFileName, L"active.json") == 0 ||
                _wcsicmp(found.cFileName, L"custom-dark.json") == 0 ||
                _wcsicmp(found.cFileName, L"custom-light.json") == 0) continue;
            ThemeEntry *entry = &g_theme_entries[g_theme_entry_count];
            ZeroMemory(entry, sizeof(*entry));
            wcsncpy(entry->file_name, found.cFileName,
                    ARRAY_LEN(entry->file_name) - 1);
            wchar_t path[MAX_PATH * 4];
            theme_file_path(entry->file_name, path, ARRAY_LEN(path));
            wchar_t *json = read_utf8_theme_file(path);
            if (!json || !json_theme_string(json, L"name", entry->name,
                                             ARRAY_LEN(entry->name))) {
                free(json);
                continue;
            }
            free(json);
            entry->kind = 3;
            ++g_theme_entry_count;
        } while (FindNextFileW(search, &found));
        FindClose(search);
    }

    g_active_theme_index = 0;
    wchar_t path[MAX_PATH * 4];
    theme_file_path(L"active.json", path, ARRAY_LEN(path));
    wchar_t *selection = read_utf8_theme_file(path);
    BOOL selected_file_missing = FALSE;
    if (selection) {
        wchar_t active[80];
        if (json_theme_string(selection, L"active", active, ARRAY_LEN(active))) {
            selected_file_missing = _wcsicmp(active, L"default") != 0;
            for (int i = 1; i < g_theme_entry_count; ++i)
                if (_wcsicmp(active, g_theme_entries[i].file_name) == 0) {
                    g_active_theme_index = i;
                    selected_file_missing = FALSE;
                    break;
                }
        }
        free(selection);
    }
    BOOL renamed_active = FALSE;
    for (int i = 1; i < g_theme_entry_count; ++i) {
        ThemeEntry *entry = &g_theme_entries[i];
        wchar_t filename[ARRAY_LEN(entry->file_name)];
        if (!theme_filename_for_name(entry->name, entry->file_name,
                                     filename, ARRAY_LEN(filename)) ||
            _wcsicmp(filename, entry->file_name) == 0) continue;
        wchar_t old_path[MAX_PATH * 4], new_path[MAX_PATH * 4];
        theme_file_path(entry->file_name, old_path, ARRAY_LEN(old_path));
        theme_file_path(filename, new_path, ARRAY_LEN(new_path));
        if (MoveFileExW(old_path, new_path, MOVEFILE_WRITE_THROUGH)) {
            wcscpy(entry->file_name, filename);
            if (i == g_active_theme_index) renamed_active = TRUE;
        }
    }
    if (renamed_active || selected_file_missing) theme_save_selection();
    theme_read_entry(&g_theme_entries[g_active_theme_index]);
    theme_refresh_selector();
    apply_active_theme();
    if (g_main && g_theme_select) update_page_visibility();
}

static void theme_select_entry(int index) {
    if (index < 0 || index >= g_theme_entry_count) return;
    g_active_theme_index = index;
    theme_read_entry(&g_theme_entries[index]);
    theme_refresh_selector();
    if (!theme_save_selection())
        MessageBoxW(g_main, L"The theme changed, but Charter could not save the selection.",
                    APP_TITLE, MB_OK | MB_ICONWARNING);
    apply_active_theme();
}

static BOOL theme_create_custom(void) {
    if (!g_themes_root[0] || g_theme_entry_count >= (int)ARRAY_LEN(g_theme_entries))
        return FALSE;
    ThemeEntry *entry = &g_theme_entries[g_theme_entry_count];
    ZeroMemory(entry, sizeof(*entry));
    entry->kind = 3;
    int number = 1;
    BOOL duplicate;
    do {
        duplicate = FALSE;
        swprintf(entry->name, ARRAY_LEN(entry->name), L"My Theme %d", number++);
        for (int i = 1; i < g_theme_entry_count; ++i)
            if (_wcsicmp(g_theme_entries[i].name, entry->name) == 0)
                duplicate = TRUE;
    } while (duplicate && number < 1000);
    if (!theme_filename_for_name(entry->name, NULL, entry->file_name,
                                 ARRAY_LEN(entry->file_name)) ||
        !theme_save_entry(entry)) return FALSE;
    int index = g_theme_entry_count++;
    theme_select_entry(index);
    return TRUE;
}

static BOOL theme_delete_selected(void) {
    if (g_active_theme_index < 1 || g_active_theme_index >= g_theme_entry_count)
        return FALSE;
    int index = g_active_theme_index;
    ThemeEntry *entry = &g_theme_entries[index];
    wchar_t question[192];
    swprintf(question, ARRAY_LEN(question),
             L"Move the theme \"%ls\" to the Recycle Bin?", entry->name);
    if (MessageBoxW(g_main, question, L"Delete theme",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return FALSE;

    /* SHFileOperation expects a double-NUL-terminated, absolute path. */
    wchar_t path[MAX_PATH * 4];
    theme_file_path(entry->file_name, path, ARRAY_LEN(path));
    size_t length = wcslen(path);
    if (length + 2 > ARRAY_LEN(path)) return FALSE;
    path[length + 1] = L'\0';
    SHFILEOPSTRUCTW operation;
    ZeroMemory(&operation, sizeof(operation));
    operation.hwnd = g_main;
    operation.wFunc = FO_DELETE;
    operation.pFrom = path;
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
                       FOF_SILENT | FOF_NOERRORUI;
    if (SHFileOperationW(&operation) != 0 || operation.fAnyOperationsAborted) {
        MessageBoxW(g_main, L"Charter could not move this theme to the Recycle Bin.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return FALSE;
    }

    for (int i = index; i + 1 < g_theme_entry_count; ++i)
        g_theme_entries[i] = g_theme_entries[i + 1];
    --g_theme_entry_count;
    ZeroMemory(&g_theme_entries[g_theme_entry_count], sizeof(ThemeEntry));
    theme_select_entry(0);
    update_page_visibility();
    return TRUE;
}

static BOOL theme_pick_color(ThemeColor color) {
    if (color < 0 || color >= TC_COUNT || !g_theme_builder_open ||
        g_active_theme_index == 0) return FALSE;
    CHOOSECOLORW chooser;
    ZeroMemory(&chooser, sizeof(chooser));
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = g_main;
    chooser.rgbResult = g_theme_colors[color];
    chooser.lpCustColors = g_theme_dialog_colors;
    chooser.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&chooser)) return FALSE;
    g_theme_colors[color] = chooser.rgbResult;
    apply_active_theme();
    return TRUE;
}

static BOOL theme_pick_font(BOOL display) {
    if (!g_theme_builder_open || g_active_theme_index == 0) return FALSE;
    LOGFONTW font;
    ZeroMemory(&font, sizeof(font));
    wcsncpy(font.lfFaceName, display ? g_theme_display_font : g_theme_body_font,
            ARRAY_LEN(font.lfFaceName) - 1);
    CHOOSEFONTW chooser;
    ZeroMemory(&chooser, sizeof(chooser));
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = g_main;
    chooser.lpLogFont = &font;
    chooser.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST;
    if (!ChooseFontW(&chooser)) return FALSE;
    wchar_t *target = display ? g_theme_display_font : g_theme_body_font;
    wcsncpy(target, font.lfFaceName, LF_FACESIZE - 1);
    target[LF_FACESIZE - 1] = L'\0';
    apply_active_theme();
    return TRUE;
}

static void apply_active_theme(void) {
    if (g_search_brush) DeleteObject(g_search_brush);
    if (g_sidebar_brush) DeleteObject(g_sidebar_brush);
    g_search_brush = CreateSolidBrush(C_SURFACE);
    g_sidebar_brush = CreateSolidBrush(C_SIDEBAR);
    create_fonts();
    if (g_list) {
        ListView_SetBkColor(g_list, C_BG);
        ListView_SetTextBkColor(g_list, C_BG);
        ListView_SetTextColor(g_list, C_TEXT);
        SetWindowTheme(g_list, g_theme_is_light ? NULL : L"DarkMode_Explorer", NULL);
        if (g_list_header)
            SetWindowTheme(g_list_header,
                           g_theme_is_light ? NULL : L"DarkMode_Explorer", NULL);
    }
    if (g_playlist_list)
        SetWindowTheme(g_playlist_list,
                       g_theme_is_light ? NULL : L"DarkMode_Explorer", NULL);
    if (g_output)
        SetWindowTheme(g_output, g_theme_is_light ? NULL : L"DarkMode_CFD", NULL);
    if (g_theme_select)
        SetWindowTheme(g_theme_select, g_theme_is_light ? NULL : L"DarkMode_CFD", NULL);
    HWND edits[] = { g_url_edit, g_search, g_discord_app_id_edit, g_theme_name_edit };
    for (size_t i = 0; i < ARRAY_LEN(edits); ++i)
        if (edits[i]) {
            /* The native dark EDIT theme can ignore WM_CTLCOLOREDIT's palette. */
            SetWindowTheme(edits[i], L"", L"");
            RedrawWindow(edits[i], NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
        }
    if (g_output) SendMessageW(g_output, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    if (g_playlist_list) SendMessageW(g_playlist_list, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_discord_app_id_edit)
        SendMessageW(g_discord_app_id_edit, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_theme_name_edit)
        SendMessageW(g_theme_name_edit, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    if (g_theme_select)
        SendMessageW(g_theme_select, WM_SETFONT, (WPARAM)g_font_body, TRUE);
    wchar_t font_button[96];
    if (g_theme_body_font_button) {
        swprintf(font_button, ARRAY_LEN(font_button), L"Body: %ls", g_theme_body_font);
        SetWindowTextW(g_theme_body_font_button, font_button);
    }
    if (g_theme_display_font_button) {
        swprintf(font_button, ARRAY_LEN(font_button), L"Display: %ls", g_theme_display_font);
        SetWindowTextW(g_theme_display_font_button, font_button);
    }
    enable_dark_app_mode();
    if (g_main) {
        BOOL dark = !g_theme_is_light;
        DwmSetWindowAttribute(g_main, 20, &dark, sizeof(dark));
        if (g_video_window)
            DwmSetWindowAttribute(g_video_window, 20, &dark, sizeof(dark));
        RedrawWindow(g_main, NULL, NULL,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        if (g_video_window) InvalidateRect(g_video_window, NULL, TRUE);
    }
}

static void theme_open_builder(void) {
    if (g_active_theme_index == 0) return;
    g_theme_builder_open = TRUE;
    g_theme_builder_scroll = 0;
    SetWindowTextW(g_theme_name_edit, g_theme_entries[g_active_theme_index].name);
    update_page_visibility();
    layout_ui(g_main);
    SetFocus(g_theme_name_edit);
}

static void theme_close_builder(BOOL save) {
    if (!g_theme_builder_open || g_active_theme_index == 0) return;
    ThemeEntry *entry = &g_theme_entries[g_active_theme_index];
    if (save) {
        wchar_t name[ARRAY_LEN(entry->name)];
        GetWindowTextW(g_theme_name_edit, name, ARRAY_LEN(name));
        wchar_t *start = name;
        while (iswspace(*start)) ++start;
        wchar_t *end = start + wcslen(start);
        while (end > start && iswspace(end[-1])) --end;
        *end = L'\0';
        if (!start[0]) {
            MessageBoxW(g_main, L"Give this theme a name before saving.",
                        APP_TITLE, MB_OK | MB_ICONINFORMATION);
            SetFocus(g_theme_name_edit);
            return;
        }
        wchar_t old_name[ARRAY_LEN(entry->name)];
        wchar_t old_file[ARRAY_LEN(entry->file_name)];
        wcscpy(old_name, entry->name);
        wcscpy(old_file, entry->file_name);
        wcsncpy(entry->name, start, ARRAY_LEN(entry->name) - 1);
        entry->name[ARRAY_LEN(entry->name) - 1] = L'\0';
        wchar_t new_file[ARRAY_LEN(entry->file_name)];
        if (!theme_filename_for_name(entry->name, old_file,
                                     new_file, ARRAY_LEN(new_file))) {
            wcscpy(entry->name, old_name);
            MessageBoxW(g_main, L"Charter could not make a unique theme filename.",
                        APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }
        BOOL renamed = _wcsicmp(new_file, old_file) != 0;
        wchar_t old_path[MAX_PATH * 4], new_path[MAX_PATH * 4];
        if (renamed) {
            theme_file_path(old_file, old_path, ARRAY_LEN(old_path));
            theme_file_path(new_file, new_path, ARRAY_LEN(new_path));
            if (!MoveFileExW(old_path, new_path, MOVEFILE_WRITE_THROUGH)) {
                wcscpy(entry->name, old_name);
                MessageBoxW(g_main, L"Charter could not rename the theme JSON file.",
                            APP_TITLE, MB_OK | MB_ICONERROR);
                return;
            }
            wcscpy(entry->file_name, new_file);
        }
        if (!theme_save_entry(entry)) {
            if (renamed) MoveFileExW(new_path, old_path, MOVEFILE_WRITE_THROUGH);
            wcscpy(entry->name, old_name);
            wcscpy(entry->file_name, old_file);
            MessageBoxW(g_main, L"Charter could not save this theme JSON file.",
                        APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }
        if (renamed && !theme_save_selection())
            MessageBoxW(g_main, L"Theme saved, but Charter could not remember its new filename.",
                        APP_TITLE, MB_OK | MB_ICONWARNING);
    } else {
        theme_read_entry(entry);
        apply_active_theme();
    }
    g_theme_builder_open = FALSE;
    theme_refresh_selector();
    update_page_visibility();
    layout_ui(g_main);
}

static BOOL theme_import_path(const wchar_t *source) {
    wchar_t *json = read_utf8_theme_file(source);
    wchar_t name[64];
    BOOL valid = json && json_theme_string(json, L"name", name, ARRAY_LEN(name)) &&
                 name[0] && wcsstr(json, L"\"colors\"");
    for (int color = 0; valid && color < TC_COUNT; ++color) {
        wchar_t hex[16];
        valid = json_theme_string(json, g_theme_color_keys[color], hex,
                                  ARRAY_LEN(hex)) && wcslen(hex) == 7 && hex[0] == L'#';
        if (valid) {
            wchar_t *end = NULL;
            wcstoul(hex + 1, &end, 16);
            valid = end && *end == L'\0';
        }
    }
    free(json);
    if (!valid) {
        MessageBoxW(g_main, L"This is not a valid Charter theme JSON file.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    for (int i = 1; i < g_theme_entry_count; ++i) {
        wchar_t existing[MAX_PATH * 4];
        theme_file_path(g_theme_entries[i].file_name, existing, ARRAY_LEN(existing));
        if (_wcsicmp(source, existing) == 0) {
            theme_select_entry(i);
            return TRUE;
        }
    }
    if (g_theme_entry_count >= (int)ARRAY_LEN(g_theme_entries)) {
        MessageBoxW(g_main, L"Charter's theme list is full (63 custom themes).",
                    APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return FALSE;
    }
    wchar_t filename[80], destination[MAX_PATH * 4];
    if (!theme_filename_for_name(name, NULL, filename, ARRAY_LEN(filename))) return FALSE;
    theme_file_path(filename, destination, ARRAY_LEN(destination));
    if (!CopyFileW(source, destination, TRUE)) {
        MessageBoxW(g_main, L"Charter could not copy this theme into its Themes folder.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    load_themes();
    for (int i = 1; i < g_theme_entry_count; ++i)
        if (_wcsicmp(filename, g_theme_entries[i].file_name) == 0) {
            theme_select_entry(i);
            return TRUE;
        }
    MessageBoxW(g_main, L"The theme was copied, but Charter could not load it.",
                APP_TITLE, MB_OK | MB_ICONWARNING);
    return FALSE;
}

static BOOL theme_import_file(void) {
    wchar_t source[MAX_PATH * 4] = L"";
    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"Theme JSON (*.json)\0*.json\0All files (*.*)\0*.*\0\0";
    dialog.lpstrFile = source;
    dialog.nMaxFile = ARRAY_LEN(source);
    dialog.lpstrInitialDir = g_themes_root;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) && theme_import_path(source);
}

static BOOL theme_export_active(void) {
    if (g_active_theme_index < 0 || g_active_theme_index >= g_theme_entry_count)
        return FALSE;
    ThemeEntry copy = g_theme_entries[g_active_theme_index];
    if (copy.kind == 0) wcscpy(copy.name, L"Default Charter");
    wchar_t filename[80], path[MAX_PATH * 4] = L"";
    if (!theme_filename_for_name(copy.name,
                                 copy.kind == 0 ? NULL : copy.file_name,
                                 filename, ARRAY_LEN(filename)))
        return FALSE;
    wcscpy(path, filename);
    OPENFILENAMEW dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_main;
    dialog.lpstrFilter = L"Theme JSON (*.json)\0*.json\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = ARRAY_LEN(path);
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) return FALSE;
    if (!theme_save_entry_to_path(&copy, path)) {
        MessageBoxW(g_main, L"Charter could not export this theme JSON file.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    return TRUE;
}

static void theme_check_for_changes(void) {
    if (g_theme_change_handle == INVALID_HANDLE_VALUE) return;
    DWORD state = WaitForSingleObject(g_theme_change_handle, 0);
    if (state == WAIT_OBJECT_0) {
        FindNextChangeNotification(g_theme_change_handle);
        g_theme_refresh_pending = TRUE;
    }
    if (g_theme_refresh_pending && !g_theme_builder_open) {
        g_theme_refresh_pending = FALSE;
        load_themes();
    }
}

static void theme_stop_watching(void) {
    if (g_theme_change_handle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(g_theme_change_handle);
        g_theme_change_handle = INVALID_HANDLE_VALUE;
    }
}
