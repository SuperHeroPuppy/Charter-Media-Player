# Charter Music Browser

A lightweight native Windows audio/video downloader, media library, playlist manager,
and dedicated in-app player written in C for Windows 10 and 11.

## Quick start

Install the MSYS2 UCRT64 GCC toolchain, then run:

```sh
bash build.sh
```

The executable is written to `build/CharterMusicBrowser.exe`.

CURRENT INTERFACE
-----------------
Charter uses a music-service style layout with a permanent left sidebar and a
persistent player across the bottom of the window.

- Library, Downloads, Settings, and playlists live in the sidebar.
- The Library shows artwork, media name, artist/creator, and file size.
- Double-clicking audio starts playback inside Charter; video opens Charter's own video window.
- Import media copies one or more external audio/video files into the Charter library.
- Delete permanently removes a selected media file, its yt-dlp sidecar, and its
  references from Charter playlists (after confirmation).
- Starred and hearted state survives refreshes. Hearts also populate the built-in
  Liked playlist in the sidebar.
- Previous, play/pause, next, seek, and volume stay in the bottom player.
- Loop and shuffle controls stay beside the main transport controls.
- Volume sits beside the playback progress bar.
- Audio output selection lives on the Settings page instead of the player bar.
- Optional Discord Rich Presence can share the current title, artist, media type,
  play/pause state, and playback timeline from the Settings page.
- Playlist names and artwork can be chosen when a playlist is created and edited
  later.
- The supplied Charter artwork and navigation icons are embedded in the app.

APP-OWNED LIBRARY
-----------------
Charter Music Browser owns its library. It automatically creates and scans:

%LOCALAPPDATA%\Charter Music Browser\Music

Playlists are stored in:

%LOCALAPPDATA%\Charter Music Browser\Playlists

Playlist artwork copied into Charter is stored in:

%LOCALAPPDATA%\Charter Music Browser\Playlist Icons

Starred/hearted state is stored in Library State.cmbstate under the Charter app-data
folder. The generated Liked.cmbpl remains compatible with the normal playlist format.
Discord presence preferences are stored in Discord Presence.cfg in the same app-data
folder. The Application ID is public configuration data; Charter stores no Discord
login, user token, or OAuth secret.

Downloader support files are cached under:

%LOCALAPPDATA%\Charter Music Browser\Tools

PLAYBACK
--------
Playback stays inside Charter. Charter never hands a file to the default Windows
media-player application.

The audio player uses a Charter-owned decoding thread and WinMM waveOut PCM output.
Windows Media Foundation handles native codecs, while Charter automatically uses its
portable FFmpeg component as a compatibility decoder when a format cannot be decoded
directly. The in-app video window uses Media Foundation rendering and the same player
transport controls.

MP3, WAV, WMA, AAC/M4A, FLAC and other Windows formats play directly. OGG, OPUS,
AIFF, APE, WebM audio, and other imported/downloaded audio formats are normalized to
temporary PCM for playback when needed. Video includes MP4/M4V, MOV, WMV, AVI, MKV,
WebM, MPEG, OGV, FLV, TS and M2TS; actual external-video codec support depends on the
decoders installed in Windows. Charter's MP4 downloads request H.264/AAC for reliable
in-app playback.

Playback features include:

- Double-click to play.
- Previous and next.
- Play/pause.
- Automatic next-track playback.
- Seeking with elapsed and total time.
- Per-app volume.
- Output-device selection on Settings.

DISCORD RICH PRESENCE
---------------------
Discord Rich Presence is optional and off by default. To enable it:

1. Create an application in the Discord Developer Portal.
2. Copy its numeric Application ID.
3. Open Settings in Charter, paste the ID, turn Discord presence on, and choose
   Save & connect.

Charter talks only to the locally running Discord desktop app. It reconnects
automatically if Discord is opened later. Disabling the setting or closing Charter
clears its activity. The application name and icon shown by Discord come from the
Developer Portal application.

LIBRARY METADATA
----------------
Charter reads Windows file metadata for each track. The main list displays:

Artwork | Name | Artist | File size

If a title is missing, the filename is used. If artist metadata is missing,
"Unknown artist" is displayed. Artwork is requested from the Windows Shell so
embedded album artwork can be shown without adding another image/metadata library.

PLAYLISTS
---------
Use New playlist in the sidebar to open the playlist editor. You can choose the
playlist name and optionally select custom artwork. Select an existing playlist
and use Edit playlist to change its name or replace its artwork.

Playlist files use the .cmbpl extension. They store playlist metadata and track
references without duplicating the audio itself.

DOWNLOADER
----------
Open Downloads from the sidebar.

- Paste any media URL or playlist that yt-dlp can resolve.
- Choose MP3, M4A, OPUS, FLAC, WAV, Original audio, or MP4 video.
- MP4 downloads can be capped at 360p, 480p, 720p, 1080p, 1440p, or 2160p,
  or left at Best available.
- Downloads are placed directly into the Charter library.
- The library refreshes after a successful download.
- yt-dlp source metadata is retained in .info.json sidecars.
- Metadata and thumbnails are embedded when supported by the chosen format.
- Download work runs outside the UI thread.

Charter passes links to the downloader instead of maintaining a hard-coded site
allowlist. DRM-protected streams still cannot be decrypted.

LIGHTWEIGHT SELF-MANAGED TOOLS
------------------------------
The end user does not need Python, pip, Node.js, Deno, WinGet packages, or a
separate music player.

Charter prepares portable downloader components under its AppData Tools folder
when downloading requires them. FFmpeg is fetched only when conversion requires
it. Playback itself uses Windows APIs and is independent of the downloader tools.

ICONS
-----
The supplied icon pack is under assets\icons. Original SVG files are preserved.
PNG-only assets were converted to SVG and also rendered to ICO resources for the
native Win32 UI.

The supplied pack does not currently contain dedicated play, pause, previous,
next, volume/speaker, or refresh icons. Charter still draws those controls itself.

KEYBOARD SHORTCUTS
------------------
Ctrl+1   Library
Ctrl+2   Downloads
Ctrl+3   Settings
Ctrl+F   Library search
Ctrl+D   Downloader URL field
Ctrl+O   Open Charter music folder
Ctrl+I   Import external audio or video
Delete   Permanently delete the selected media item (with confirmation)
F5       Refresh library
Space    Play/pause when a text field is not focused

PROJECT LAYOUT
--------------

```text
.
|-- .github/workflows/   GitHub Actions build
|-- assets/              Application artwork and icon sources
|-- docs/                Architecture documentation
|-- resources/           Win32 resource script, manifest, and resource IDs
|-- src/                 C source and focused unity-build modules
|-- build.bat            Command Prompt build
|-- build.sh             MSYS2 build
`-- Makefile             Optional make-based build
```

The source uses a deliberate unity build: `src/main.c` includes the focused
`src/*.inc.c` implementation files into one translation unit. This keeps the
application's private Win32 state internal without returning to one enormous
source file. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the module map.

BUILDING WITH MSYS2 UCRT64
--------------------------
From the CharterMusicBrowser directory:

```sh
bash build.sh
```

Alternatively:

```sh
make
```

Manual command:

```sh
mkdir -p build
windres resources/CharterMusicBrowser.rc -I resources -O coff \
    -o build/CharterMusicBrowser_resources.o

gcc src/main.c build/CharterMusicBrowser_resources.o \
    -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic \
    -municode -mwindows \
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm \
    -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 \
    -o build/CharterMusicBrowser.exe

rm -f build/CharterMusicBrowser_resources.o
```

BUILDING FROM CMD
-----------------
Run `build.bat`.

The finished program is `build/CharterMusicBrowser.exe`.

GITHUB
------

Generated files are excluded by `.gitignore`, text and binary attributes are
defined, and `.github/workflows/build.yml` builds and uploads the Windows
executable for pushes and pull requests.

No license has been selected for the project. Choose and add the license you want
before publishing if other people should be allowed to reuse or redistribute the
source.
