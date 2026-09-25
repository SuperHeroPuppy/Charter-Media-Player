/* Per-user Windows registration for Open With and Default Apps. */

static BOOL registry_set_string(HKEY root, const wchar_t *subkey,
                                const wchar_t *name, const wchar_t *value) {
    HKEY key = NULL;
    DWORD disposition = 0;
    LSTATUS status = RegCreateKeyExW(root, subkey, 0, NULL,
                                    REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                    NULL, &key, &disposition);
    (void)disposition;
    if (status != ERROR_SUCCESS || !key) return FALSE;
    DWORD bytes = (DWORD)((wcslen(value) + 1) * sizeof(wchar_t));
    status = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, bytes);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

static BOOL register_media_file_handlers(void) {
    static const wchar_t *extensions[] = {
        L".mp3", L".wav", L".wma", L".m4a", L".aac", L".flac",
        L".ogg", L".oga", L".opus", L".mid", L".midi", L".aiff",
        L".aif", L".ape", L".webm", L".weba", L".mka", L".mp4",
        L".m4v", L".mov", L".wmv", L".avi", L".mkv", L".mpeg",
        L".mpg", L".ogv", L".flv", L".ts", L".m2ts"
    };
    const wchar_t *prog_id = L"SuperNetwork.CharterMediaPlayer.Media";
    const wchar_t *capabilities =
        L"Software\\Super's Network\\Charter Media Player\\Capabilities";
    wchar_t executable[MAX_PATH * 4];
    DWORD length = GetModuleFileNameW(NULL, executable, ARRAY_LEN(executable));
    if (!length || length >= ARRAY_LEN(executable)) return FALSE;

    wchar_t command[MAX_PATH * 4 + 16];
    wchar_t icon[MAX_PATH * 4 + 8];
    swprintf(command, ARRAY_LEN(command), L"\"%ls\" \"%%1\"", executable);
    swprintf(icon, ARRAY_LEN(icon), L"%ls,0", executable);

    BOOL ok = TRUE;
    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\Classes\\SuperNetwork.CharterMediaPlayer.Media",
        NULL, L"Charter Media Player media") && ok;
    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\Classes\\SuperNetwork.CharterMediaPlayer.Media\\DefaultIcon",
        NULL, icon) && ok;
    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\Classes\\SuperNetwork.CharterMediaPlayer.Media\\shell\\open\\command",
        NULL, command) && ok;

    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\Classes\\Applications\\CharterMediaPlayer.exe",
        L"FriendlyAppName", APP_TITLE) && ok;
    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\Classes\\Applications\\CharterMediaPlayer.exe\\shell\\open\\command",
        NULL, command) && ok;

    ok = registry_set_string(HKEY_CURRENT_USER, capabilities,
        L"ApplicationName", APP_TITLE) && ok;
    ok = registry_set_string(HKEY_CURRENT_USER, capabilities,
        L"ApplicationDescription",
        L"Play audio and video directly without importing it into the Charter library.") && ok;
    ok = registry_set_string(HKEY_CURRENT_USER, capabilities,
        L"ApplicationIcon", icon) && ok;

    for (size_t i = 0; i < ARRAY_LEN(extensions); ++i) {
        wchar_t association_key[512];
        wchar_t supported_key[512];
        swprintf(association_key, ARRAY_LEN(association_key),
                 L"%ls\\FileAssociations", capabilities);
        swprintf(supported_key, ARRAY_LEN(supported_key),
                 L"Software\\Classes\\Applications\\CharterMediaPlayer.exe\\SupportedTypes");
        ok = registry_set_string(HKEY_CURRENT_USER, association_key,
                                 extensions[i], prog_id) && ok;
        ok = registry_set_string(HKEY_CURRENT_USER, supported_key,
                                 extensions[i], L"") && ok;
    }

    ok = registry_set_string(HKEY_CURRENT_USER,
        L"Software\\RegisteredApplications", APP_REGISTERED_NAME,
        capabilities) && ok;
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return ok;
}

static void open_default_apps_settings(void) {
    HINSTANCE opened = ShellExecuteW(
        g_main, L"open",
        L"ms-settings:defaultapps?registeredAppUser=Charter%20Media%20Player",
        NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)opened <= 32)
        ShellExecuteW(g_main, L"open", L"ms-settings:defaultapps",
                      NULL, NULL, SW_SHOWNORMAL);
}
