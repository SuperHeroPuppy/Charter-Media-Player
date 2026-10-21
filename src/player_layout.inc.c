/* One source of truth for transport buttons and the aligned progress bar. */
typedef struct PlayerControlsLayout {
    RECT loop, previous, play, next, shuffle, seek, volume;
    int center;
    BOOL compact;
} PlayerControlsLayout;

static RECT player_centered_rect(int x, int y, int width, int height) {
    return (RECT){ x - width / 2, y - height / 2,
                   x - width / 2 + width, y - height / 2 + height };
}

static PlayerControlsLayout player_controls_layout(int width, int player_top,
                                                    int sidebar) {
    PlayerControlsLayout result;
    ZeroMemory(&result, sizeof(result));
    result.center = width / 2;
    result.compact = width < S(1000);
    int small = S(36), large = S(44), step = S(52);
    int center_y = player_top + S(13) + small / 2;
    result.loop = player_centered_rect(result.center - 2 * step, center_y,
                                       small, small);
    result.previous = player_centered_rect(result.center - step, center_y,
                                           small, small);
    result.play = player_centered_rect(result.center, center_y,
                                       large, large);
    result.next = player_centered_rect(result.center + step, center_y,
                                       small, small);
    result.shuffle = player_centered_rect(result.center + 2 * step, center_y,
                                          small, small);
    int seek_width = result.compact
        ? max(S(180), min(S(410), width - sidebar - S(100))) : S(410);
    result.seek = player_centered_rect(result.center,
                                       player_top + S(73), seek_width, S(24));
    int volume_x = result.center + S(264), volume_y = player_top + S(61);
    result.volume = (RECT){ volume_x, volume_y,
                            volume_x + S(118), volume_y + S(24) };
    return result;
}

static void move_player_control(HWND control, RECT area) {
    MoveWindow(control, area.left, area.top,
               area.right - area.left, area.bottom - area.top, TRUE);
}
