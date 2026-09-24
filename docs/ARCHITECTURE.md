# Architecture

Charter Music Browser is a native Win32 C application. Its source is organized as
a deliberate unity build: `src/main.c` includes focused implementation fragments
into one translation unit.

This keeps the existing application-wide Win32 state private while making each
feature area independently navigable. It also avoids publishing a large internal
ABI made only of global window handles and callback helpers.

## Source map

| File | Responsibility |
| --- | --- |
| `src/charter_internal.h` | Platform headers, IDs, shared types, private state, and forward declarations |
| `src/core.inc.c` | General helpers, metadata extraction, thumbnails, and track allocation |
| `src/downloads.inc.c` | yt-dlp/FFmpeg preparation, download workers, formats, and resolutions |
| `src/library.inc.c` | Library scans, playlists, liked/starred state, and deletion |
| `src/video.inc.c` | MFPlay callbacks and the in-app video window |
| `src/playback.inc.c` | Audio decoding, waveOut playback, transport controls, and Discord IPC |
| `src/app_lifecycle.inc.c` | App-data paths, refresh flow, folder actions, and imports |
| `src/ui_controls.inc.c` | Fonts, drawing helpers, buttons, inputs, sliders, and list customization |
| `src/ui_shell.inc.c` | Main layout, settings, dialogs, painting, and the primary window procedure |
| `src/main.c` | Unity assembly, window-class registration, initialization, and `wWinMain` |

## Runtime data

User media, playlists, downloaded tools, and preferences live under
`%LOCALAPPDATA%\Charter Music Browser`. They are not stored in the repository.

## Build flow

The resource compiler embeds the manifest, application icon, and UI icons. GCC
then compiles `src/main.c`; its included fragments are not compiled separately.
This is why their filenames end in `.inc.c`.

Both build scripts and the Makefile place generated files under `build/`, which is
excluded from version control.
