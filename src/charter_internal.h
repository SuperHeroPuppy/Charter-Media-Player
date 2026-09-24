#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <oleauto.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <urlmon.h>
#include <wchar.h>
#include <wctype.h>
#include <stdint.h>
#include <time.h>
#include "../resources/resource.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "msimg32.lib")
#endif

#define APP_CLASS        L"CharterMusicBrowserWindow"
#define BUTTON_CLASS     L"CharterMusicBrowserButton"
#define SEARCH_CLASS     L"CharterMusicBrowserSearch"
#define INPUT_CLASS      L"CharterMusicBrowserInput"
#define SLIDER_CLASS     L"CharterMusicBrowserSlider"
#define PLAYLIST_DIALOG_CLASS L"CharterMusicBrowserPlaylistDialog"
#define VIDEO_CLASS      L"CharterMusicBrowserVideoWindow"
#define APP_TITLE        L"Charter Music Browser"

#define ID_OPEN_LIBRARY   1001
#define ID_SEARCH         1002
#define ID_LIST           1003
#define ID_PLAY           1004
#define ID_PAUSE          1005
#define ID_STOP           1006
#define ID_OPEN_FOLDER    1007
#define ID_REFRESH        1008
#define ID_URL            1009
#define ID_DOWNLOAD       1010
#define ID_FORMAT         1011
#define ID_INSTALL_TOOLS  1012
#define ID_NAV_LIBRARY    1013
#define ID_NAV_DOWNLOADS  1014
#define ID_NEW_PLAYLIST   1015
#define ID_PLAYLIST_LIST  1016
#define ID_ADD_PLAYLIST   1017
#define ID_REMOVE_PLAYLIST 1018
#define ID_PREVIOUS       1019
#define ID_NEXT           1020
#define ID_SEEK           1021
#define ID_VOLUME         1022
#define ID_OUTPUT         1023
#define ID_NAV_SETTINGS   1024
#define ID_EDIT_PLAYLIST  1025
#define ID_REFRESH_OUTPUTS 1026
#define ID_IMPORT_MEDIA   1027
#define ID_VIDEO_PLAY     1028
#define ID_VIDEO_SEEK     1029
#define ID_DELETE_MEDIA   1030
#define ID_LOOP           1031
#define ID_SHUFFLE        1032
#define ID_STAR_MEDIA     1033
#define ID_HEART_MEDIA    1034
#define ID_RESOLUTION     1035
#define ID_DISCORD_TOGGLE 1036
#define ID_DISCORD_APP_ID 1037
#define ID_DISCORD_SAVE   1038

#define ID_FMT_MP3       2001
#define ID_FMT_M4A       2002
#define ID_FMT_OPUS      2003
#define ID_FMT_FLAC      2004
#define ID_FMT_WAV       2005
#define ID_FMT_ORIGINAL  2006
#define ID_FMT_MP4       2007
#define ID_RES_BEST      2010
#define ID_RES_2160      2011
#define ID_RES_1440      2012
#define ID_RES_1080      2013
#define ID_RES_720       2014
#define ID_RES_480       2015
#define ID_RES_360       2016

#define WM_APP_DOWNLOAD_UPDATE (WM_APP + 20)
#define WM_APP_DOWNLOAD_DONE   (WM_APP + 21)
#define WM_APP_TOOLS_DONE      (WM_APP + 22)
#define WM_APP_AUDIO_COMPLETE  (WM_APP + 40)
#define WM_APP_SLIDER_CHANGED  (WM_APP + 41)
#define WM_APP_VIDEO_COMPLETE  (WM_APP + 42)

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Windows 11-inspired dark palette with Charter gold as the app accent. */
#define C_BG             RGB(32, 32, 32)
#define C_CARD           RGB(43, 43, 43)
#define C_CARD_ALT       RGB(47, 47, 47)
#define C_SURFACE        RGB(50, 50, 50)
#define C_SURFACE_HOVER  RGB(58, 58, 58)
#define C_SURFACE_PRESS  RGB(64, 64, 64)
#define C_BORDER         RGB(70, 70, 70)
#define C_BORDER_SOFT    RGB(58, 58, 58)
#define C_TEXT           RGB(245, 245, 245)
#define C_TEXT_DIM       RGB(174, 174, 174)
#define C_TEXT_FAINT     RGB(132, 132, 132)
#define C_ACCENT         RGB(239, 159, 9)
#define C_ACCENT_HOVER   RGB(247, 174, 37)
#define C_ACCENT_PRESS   RGB(220, 143, 0)
#define C_ACCENT_TEXT    RGB(28, 22, 12)
#define C_SELECTED       RGB(57, 57, 57)
#define C_HEADER         RGB(38, 38, 38)
#define C_SIDEBAR        RGB(24, 24, 24)
#define C_PLAYER         RGB(26, 26, 26)
#define C_PLAYER_TOP     RGB(52, 52, 52)

#define BTN_HOVER        0x01
#define BTN_PRESSED      0x02

typedef struct Track {
    wchar_t *path;
    wchar_t *title;
    wchar_t *artist;
    wchar_t *folder;
    wchar_t extension[16];
    ULONGLONG size_bytes;
    int image_index;
    BOOL is_video;
    BOOL starred;
    BOOL liked;
} Track;

typedef struct PlaylistInfo {
    wchar_t name[128];
    wchar_t path[MAX_PATH * 4];
    wchar_t icon_path[MAX_PATH * 4];
    HBITMAP artwork;
    BOOL is_builtin;
} PlaylistInfo;

typedef struct MediaFlagEntry {
    wchar_t *path;
    BOOL starred;
    BOOL liked;
} MediaFlagEntry;

typedef struct AudioOutput {
    wchar_t name[256];
    UINT device_id;
} AudioOutput;

enum AppPage {
    PAGE_LIBRARY = 0,
    PAGE_DOWNLOADS = 1,
    PAGE_PLAYLIST = 2,
    PAGE_SETTINGS = 3
};

static HINSTANCE g_instance;
static HICON g_app_icon;
static HICON g_app_icon_small;
static HICON g_icon_home;
static HICON g_icon_downloads;
static HICON g_icon_settings;
static HICON g_icon_folder;
static HICON g_icon_add;
static HICON g_icon_trash;
static HICON g_icon_pencil;
static HICON g_icon_search;
static HICON g_icon_download;
static HICON g_icon_save;
static HICON g_icon_close;
static HICON g_icon_refresh;
static HICON g_icon_loop;
static HICON g_icon_shuffle;
static HICON g_icon_star;
static HICON g_icon_heart;
static HICON g_icon_mute;
static HICON g_icon_musical;
static HICON g_icon_youtube;
static HWND g_main;
static HWND g_open_library;
static HWND g_refresh;
static HWND g_search_box;
static HWND g_search;
static HWND g_list;
static HWND g_play;
static HWND g_open_folder;
static HWND g_list_header;
static HWND g_url_box;
static HWND g_url_edit;
static HWND g_download;
static HWND g_format;
static HWND g_install_tools;
static HWND g_nav_library;
static HWND g_nav_downloads;
static HWND g_nav_settings;
static HWND g_new_playlist;
static HWND g_playlist_list;
static HWND g_add_playlist;
static HWND g_remove_playlist;
static HWND g_edit_playlist;
static HWND g_refresh_outputs;
static HWND g_import_media;
static HWND g_previous;
static HWND g_next;
static HWND g_seek;
static HWND g_volume;
static HWND g_output;
static HWND g_video_window;
static HWND g_video_surface;
static HWND g_video_play;
static HWND g_video_seek;
static HWND g_delete_media;
static HWND g_loop;
static HWND g_shuffle;
static HWND g_star_media;
static HWND g_heart_media;
static HWND g_resolution;
static HWND g_discord_toggle;
static HWND g_discord_app_id_edit;
static HWND g_discord_save;

static HFONT g_font_body;
static HFONT g_font_body_semibold;
static HFONT g_font_small;
static HFONT g_font_small_semibold;
static HFONT g_font_title;
static HFONT g_font_section;
static HBRUSH g_search_brush;
static HBRUSH g_sidebar_brush;

static Track *g_tracks;
static size_t g_track_count;
static size_t g_track_capacity;
static size_t g_visible_count;
static wchar_t g_library_root[MAX_PATH * 4] = L"Preparing library...";
static wchar_t g_app_data_root[MAX_PATH * 4] = L"";
static wchar_t g_tools_root[MAX_PATH * 4] = L"";
static wchar_t g_playlists_root[MAX_PATH * 4] = L"";
static wchar_t g_playlist_icons_root[MAX_PATH * 4] = L"";
static wchar_t g_media_flags_path[MAX_PATH * 4] = L"";
static wchar_t g_discord_config_path[MAX_PATH * 4] = L"";
static wchar_t g_ytdlp_path[MAX_PATH * 4] = L"";
static wchar_t g_ffmpeg_path[MAX_PATH * 4] = L"";
static wchar_t g_qjs_path[MAX_PATH * 4] = L"";
static wchar_t g_download_format[24] = L"MP3";
static wchar_t g_video_resolution[24] = L"BEST";
static wchar_t g_download_status[1024] = L"Ready. Paste a link to add audio or video to the Charter library.";
static wchar_t g_status_text[1024] = L"Preparing the Charter music library...";
static BOOL g_library_ready;
static BOOL g_is_playing;
static BOOL g_is_paused;
static enum AppPage g_page = PAGE_LIBRARY;
static size_t g_current_track_index = (size_t)-1;
static int g_volume_percent = 78;
static BOOL g_seek_dragging;
static BOOL g_loop_enabled;
static BOOL g_shuffle_enabled;
static BOOL g_discord_enabled;
static wchar_t g_discord_app_id[32] = L"";
static wchar_t g_discord_status[256] = L"Disabled";
static HANDLE g_discord_pipe = INVALID_HANDLE_VALUE;
static ULONGLONG g_discord_last_attempt;
static ULONGLONG g_discord_last_update;
static volatile LONG g_discord_dirty = 1;
static ULONGLONG g_discord_nonce;
static PlaylistInfo g_playlists[128];
static int g_playlist_count;
static int g_active_playlist = -1;
static wchar_t **g_playlist_tracks;
static size_t g_playlist_track_count;
static AudioOutput g_audio_outputs[64];
static int g_audio_output_count;
static int g_audio_output_index;
static MediaFlagEntry *g_media_flags;
static size_t g_media_flag_count;
static HWAVEOUT g_waveout;
static HANDLE g_playback_thread;
static HANDLE g_playback_stop_event;
static CRITICAL_SECTION g_audio_lock;
static volatile LONG64 g_player_position;
static volatile LONG64 g_player_duration;
static volatile LONG g_playback_failed;
static IMFPMediaPlayer *g_video_player;
static HIMAGELIST g_track_images;
static UINT g_dpi = 96;
static WNDPROC g_old_search_edit_proc;
static WNDPROC g_old_url_edit_proc;
static HANDLE g_download_process;
static HANDLE g_download_thread;
static HANDLE g_tools_thread;
static volatile LONG g_downloading;
static volatile LONG g_installing_tools;
static volatile LONG g_pending_download;
static volatile LONG g_pending_need_ffmpeg;
static volatile LONG g_pending_playback;
static size_t g_pending_playback_index = (size_t)-1;
static LONGLONG g_pending_playback_position;
static BOOL g_pending_playback_paused;
static int g_download_percent = -1;
static wchar_t g_last_download_error[1024] = L"";

typedef struct DownloadUpdate {
    int percent;
    wchar_t text[768];
} DownloadUpdate;

static BOOL create_directory_if_needed(const wchar_t *path);
static void start_install_tools(void);
static Track *selected_track(void);
static void populate_list(void);
static void refresh_library(void);
static void update_button_enabled_state(void);
static void layout_ui(HWND hwnd);
static void update_page_visibility(void);
static void slider_set_value(HWND hwnd, int value);
static BOOL show_playlist_editor(int playlist_index);
static void stop_playback(void);
static void pause_resume(void);
static void load_discord_config(void);
static BOOL save_discord_config(void);
static void discord_tick(void);
static void discord_disconnect(BOOL clear_presence);
static void discord_mark_dirty(void);
