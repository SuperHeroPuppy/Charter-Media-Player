/* Persistent, user-editable palettes. The built-in Charter palette is immutable. */

typedef struct ThemeEntry {
    wchar_t name[64];
    wchar_t file_name[80];
    int kind; /* 0: built-in, 1: accent dark, 2: accent light, 3: full custom */
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

static BOOL json_theme_string(const wchar_t *json, const wchar_t *key,
                              wchar_t *out, size_t capacity) {
    if (!json || !capacity) return FALSE;
    wchar_t needle[80];
    swprintf(needle, ARRAY_LEN(needle), L"\"%ls\"", key);
    const wchar_t *at = wcsstr(json, needle);
    if (!at) return FALSE;
    at = wcschr(at + wcslen(needle), L':');
    if (!at) return FALSE;
    ++at;
    while (iswspace(*at)) ++at;
    if (*at++ != L'"') return FALSE;
    size_t used = 0;
    while (*at && *at != L'"') {
        wchar_t c = *at++;
        if (c == L'\\') {
            c = *at++;
            if (c == L'n' || c == L'r' || c == L't') c = L' ';
            else if (c != L'"' && c != L'\\') return FALSE;
        }
        if (used + 1 >= capacity) return FALSE;
        out[used++] = c;
    }
    if (*at != L'"') return FALSE;
    out[used] = L'\0';
    return TRUE;
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
    theme_set_base_palette(entry->kind == 2);
    wchar_t path[MAX_PATH * 4];
    theme_file_path(entry->file_name, path, ARRAY_LEN(path));
    wchar_t *json = read_utf8_theme_file(path);
    if (!json) return FALSE;
    if (entry->kind == 3) {
        wchar_t value[32];
        if (json_theme_string(json, L"base", value, ARRAY_LEN(value)) &&
            _wcsicmp(value, L"light") == 0) theme_set_base_palette(TRUE);
        json_theme_string(json, L"bodyFont", g_theme_body_font,
                          ARRAY_LEN(g_theme_body_font));
        json_theme_string(json, L"displayFont", g_theme_display_font,
                          ARRAY_LEN(g_theme_display_font));
    }
    for (int color = 0; color < TC_COUNT; ++color) {
        if (entry->kind != 3 && (color < TC_ACCENT || color > TC_ACCENT_TEXT)) continue;
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

static BOOL theme_save_entry(const ThemeEntry *entry) {
    if (!entry || entry->kind == 0) return FALSE;
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
    wchar_t path[MAX_PATH * 4];
    theme_file_path(entry->file_name, path, ARRAY_LEN(path));
    return write_utf8_theme_file(path, json);
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
    g_theme_entry_count = 3;
    g_theme_entries[0] = (ThemeEntry){ L"Default Charter (locked)", L"", 0 };
    g_theme_entries[1] = (ThemeEntry){ L"Custom Dark", L"custom-dark.json", 1 };
    g_theme_entries[2] = (ThemeEntry){ L"Custom Light", L"custom-light.json", 2 };
    for (int index = 1; index <= 2; ++index) {
        wchar_t path[MAX_PATH * 4];
        theme_file_path(g_theme_entries[index].file_name, path, ARRAY_LEN(path));
        if (!file_exists(path)) {
            theme_set_base_palette(index == 2);
            theme_save_entry(&g_theme_entries[index]);
        }
    }

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
    if (selection) {
        wchar_t active[80];
        if (json_theme_string(selection, L"active", active, ARRAY_LEN(active))) {
            for (int i = 1; i < g_theme_entry_count; ++i)
                if (_wcsicmp(active, g_theme_entries[i].file_name) == 0) {
                    g_active_theme_index = i;
                    break;
                }
        }
        free(selection);
    }
    theme_read_entry(&g_theme_entries[g_active_theme_index]);
    theme_refresh_selector();
    apply_active_theme();
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
    swprintf(entry->name, ARRAY_LEN(entry->name), L"My Theme %d",
             g_theme_entry_count - 2);
    BOOL unique = FALSE;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        swprintf(entry->file_name, ARRAY_LEN(entry->file_name),
                 L"theme-%llu-%u.json", (unsigned long long)GetTickCount64(), attempt);
        wchar_t path[MAX_PATH * 4];
        theme_file_path(entry->file_name, path, ARRAY_LEN(path));
        if (!file_exists(path)) { unique = TRUE; break; }
    }
    if (!unique || !theme_save_entry(entry)) return FALSE;
    int index = g_theme_entry_count++;
    theme_select_entry(index);
    return TRUE;
}

static BOOL theme_delete_selected(void) {
    if (g_active_theme_index < 3 || g_active_theme_index >= g_theme_entry_count)
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

static BOOL theme_pick_color(ThemeColor color, BOOL builder) {
    if (color < 0 || color >= TC_COUNT || g_active_theme_index == 0) return FALSE;
    if (builder && g_active_theme_index < 3) return FALSE;
    if (!builder && (color < TC_ACCENT || color > TC_ACCENT_TEXT)) return FALSE;
    CHOOSECOLORW chooser;
    ZeroMemory(&chooser, sizeof(chooser));
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = g_main;
    chooser.rgbResult = g_theme_colors[color];
    chooser.lpCustColors = g_theme_dialog_colors;
    chooser.Flags = CC_RGBINIT | CC_FULLOPEN;
    if (!ChooseColorW(&chooser)) return FALSE;
    COLORREF previous = g_theme_colors[color];
    g_theme_colors[color] = chooser.rgbResult;
    if (!builder && !theme_save_entry(&g_theme_entries[g_active_theme_index])) {
        g_theme_colors[color] = previous;
        MessageBoxW(g_main, L"Charter could not save this accent color.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return FALSE;
    }
    apply_active_theme();
    return TRUE;
}

static BOOL theme_pick_font(BOOL display) {
    if (!g_theme_builder_open || g_active_theme_index < 3) return FALSE;
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
    if (g_active_theme_index < 3) return;
    g_theme_builder_open = TRUE;
    g_theme_builder_scroll = 0;
    SetWindowTextW(g_theme_name_edit, g_theme_entries[g_active_theme_index].name);
    update_page_visibility();
    layout_ui(g_main);
    SetFocus(g_theme_name_edit);
}

static void theme_close_builder(BOOL save) {
    if (!g_theme_builder_open || g_active_theme_index < 3) return;
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
        wcscpy(old_name, entry->name);
        wcsncpy(entry->name, start, ARRAY_LEN(entry->name) - 1);
        entry->name[ARRAY_LEN(entry->name) - 1] = L'\0';
        if (!theme_save_entry(entry)) {
            wcscpy(entry->name, old_name);
            MessageBoxW(g_main, L"Charter could not save this theme JSON file.",
                        APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }
    } else {
        theme_read_entry(entry);
        apply_active_theme();
    }
    g_theme_builder_open = FALSE;
    theme_refresh_selector();
    update_page_visibility();
    layout_ui(g_main);
}
