/* Opt-in, best-effort global capture of standard keyboard media keys. */
static int priority_media_command_for_key(DWORD key) {
    switch (key) {
        case VK_MEDIA_PLAY_PAUSE: return APPCOMMAND_MEDIA_PLAY_PAUSE;
        case VK_MEDIA_NEXT_TRACK: return APPCOMMAND_MEDIA_NEXTTRACK;
        case VK_MEDIA_PREV_TRACK: return APPCOMMAND_MEDIA_PREVIOUSTRACK;
        case VK_MEDIA_STOP: return APPCOMMAND_MEDIA_STOP;
        case VK_VOLUME_UP: return APPCOMMAND_VOLUME_UP;
        case VK_VOLUME_DOWN: return APPCOMMAND_VOLUME_DOWN;
        case VK_VOLUME_MUTE: return APPCOMMAND_VOLUME_MUTE;
        default: return 0;
    }
}

static LRESULT CALLBACK priority_media_keyboard_proc(int code, WPARAM message,
                                                     LPARAM data) {
    if (code != HC_ACTION || !g_priority_media_keys_enabled || !g_main)
        return CallNextHookEx(g_priority_media_keys_hook, code, message, data);

    const KBDLLHOOKSTRUCT *key = (const KBDLLHOOKSTRUCT *)data;
    int command = priority_media_command_for_key(key->vkCode);
    if (!command)
        return CallNextHookEx(g_priority_media_keys_hook, code, message, data);

    unsigned int bit = 1u << (key->vkCode - VK_VOLUME_MUTE);
    if (message == WM_KEYUP || message == WM_SYSKEYUP) {
        BOOL was_captured = (g_priority_media_keys_pressed & bit) != 0;
        g_priority_media_keys_pressed &= ~bit;
        return was_captured ? 1 : CallNextHookEx(g_priority_media_keys_hook,
                                                  code, message, data);
    }
    if (message != WM_KEYDOWN && message != WM_SYSKEYDOWN)
        return CallNextHookEx(g_priority_media_keys_hook, code, message, data);

    if (!(g_playing_path[0] && (g_is_playing || g_is_paused)))
        return CallNextHookEx(g_priority_media_keys_hook, code, message, data);

    BOOL repeat = (g_priority_media_keys_pressed & bit) != 0;
    BOOL volume_step = key->vkCode == VK_VOLUME_UP || key->vkCode == VK_VOLUME_DOWN;
    if ((!repeat || volume_step) &&
        !PostMessageW(g_main, WM_APP_PRIORITY_MEDIA_KEY, (WPARAM)command, 0))
        return CallNextHookEx(g_priority_media_keys_hook, code, message, data);
    g_priority_media_keys_pressed |= bit;
    return 1;
}

static void stop_priority_media_keys(void) {
    if (g_priority_media_keys_hook) {
        UnhookWindowsHookEx(g_priority_media_keys_hook);
        g_priority_media_keys_hook = NULL;
    }
    g_priority_media_keys_pressed = 0;
}

static BOOL apply_priority_media_keys(void) {
    if (!g_priority_media_keys_enabled) {
        stop_priority_media_keys();
        return TRUE;
    }
    if (g_priority_media_keys_hook) return TRUE;
    g_priority_media_keys_pressed = 0;
    g_priority_media_keys_hook = SetWindowsHookExW(
        WH_KEYBOARD_LL, priority_media_keyboard_proc, g_instance, 0);
    return g_priority_media_keys_hook != NULL;
}
