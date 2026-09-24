# Charter Media Player

A lightweight native Windows audio/video downloader, media library, playlist manager,
and dedicated in-app player written in C for Windows 10 and 11.

## Quick start

Install the MSYS2 UCRT64 GCC toolchain, then run:

```sh
bash build.sh
```

The executable is written to `build/CharterMediaPlayer.exe`.

CURRENT INTERFACE
-----------------
Charter uses a music-service style layout with a permanent left sidebar and a
persistent player across the bottom of the window.

- Library, Downloads, Settings, and playlists live in the sidebar.
- The Library shows a dedicated artwork column, title with file size beneath it,
  artist/creator, and per-row actions for star, like, playlist, folder, and delete.
- Ctrl-click selects multiple rows. Actions such as star, like, add to playlist,
  and delete apply to the full selection.
- Drag rows to save a custom Library or playlist order.
- Double-clicking audio starts playback inside Charter; video opens Charter's own video window.
- Import media copies one or more external audio/video files into the Charter library.
- Delete permanently removes every selected media file, its yt-dlp sidecar, and
  its references from Charter playlists after one confirmation.
- Starred and liked state survives refreshes. Starred media is highlighted and
  pinned above unstarred media. Likes fill the heart action and populate the
  built-in Liked playlist in the sidebar.
- Previous, play/pause, next, seek, and volume stay in the bottom player.
- Loop and shuffle controls stay beside the main transport controls.
- Volume sits beside the playback progress bar.
- Audio output selection lives on the Settings page instead of the player bar.
- Optional Discord Rich Presence can share the current title, artist, media type,
  play/pause state, and playback timeline. Settings can use Charter's provided
  Application ID or a custom Discord Application ID.
- The experimental compact background window is temporarily disabled; normal
  Windows minimize and restore behavior is used while that design is revisited.
- Playlist names and artwork can be chosen when a playlist is created and edited
  later.
- The supplied Charter artwork and navigation icons are embedded in the app.

APP-OWNED LIBRARY
-----------------
Charter Media Player owns its library. It automatically creates and scans:

%LOCALAPPDATA%\Charter Media Player\Music

Playlists are stored in:

%LOCALAPPDATA%\Charter Media Player\Playlists

Playlist artwork copied into Charter is stored in:

%LOCALAPPDATA%\Charter Media Player\Playlist Icons

Starred/liked state is stored in Library State.cmbstate under the Charter app-data
folder. The generated Liked.cmbpl remains compatible with the normal playlist format.
Custom Library row order is stored in Library Order.cmborder.
Discord presence preferences are stored in Discord Presence.cfg in the same app-data
folder. The Application ID is public configuration data; Charter stores no Discord
login, user token, or OAuth secret.

Volume, audio-output choice, loop, and shuffle preferences are stored in
Player Settings.cfg and restored when Charter starts. If a saved output device is
temporarily unavailable, Charter uses the system default while retaining the saved
device choice for a later refresh or restart.

### Migrating from Charter Music Browser

An upgraded legacy installation initially continues using
`%LOCALAPPDATA%\Charter Music Browser`, so its media and preferences remain
available. Open Settings and choose **Migrate & restart**. Charter then closes,
moves the complete legacy folder to `%LOCALAPPDATA%\Charter Media Player`, updates
absolute paths stored in playlists, likes/stars, and Library ordering, and restarts
from the new location. The migration control is disabled when no legacy storage is
present.

Downloader support files are cached under:

%LOCALAPPDATA%\Charter Media Player\Tools

PLAYBACK
--------
Playback stays inside Charter. Charter never hands a file to the default Windows
media-player application.

The active player owns its own path and metadata snapshot. Library and playlist
refreshes can rebuild their UI state without stopping or restarting the audio/video
that is already playing.

The audio player uses a Charter-owned decoding thread and WinMM waveOut PCM output.
Windows Media Foundation handles native codecs, while Charter automatically uses its
portable FFmpeg component as a compatibility decoder when a format cannot be decoded
directly. The in-app video window uses Media Foundation for the picture and Charter's
audio path for sound, so videos follow the output device selected in Settings too.

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
- Full-screen video using the button, F11, Alt+Enter, or a double-click.
- Auto-hiding video controls that return when the pointer moves.

DISCORD RICH PRESENCE
---------------------
Discord Rich Presence is optional and off by default. To enable it:

1. Open Settings and choose **Provided ID** to use Charter's bundled public
   Application ID (`1552645707192733697`), or choose **Custom ID** and paste your
   own numeric Discord Application ID.
2. Turn Discord presence on and choose **Save & connect**.

Charter talks only to the locally running Discord desktop app. It reconnects
automatically if Discord is opened later. Disabling the setting or closing Charter
clears its activity. The application name and icon shown by Discord come from the
Developer Portal application.

LIBRARY METADATA
----------------
Charter reads Windows file metadata for each track. The main list displays:

Icon | Name (with file size underneath) | Artist | Actions

If a title is missing, the filename is used. If artist metadata is missing,
"Unknown artist" is displayed. Artwork is requested from the Windows Shell so
embedded album artwork can be shown without adding another image/metadata library.

Large libraries populate from the filesystem immediately. Charter loads tags and
artwork on a background thread in small batches, so hundreds of items do not block
the window. Unchanged results are cached for the session and reused by later
refreshes; changing a file's size or last-write time invalidates its cached details.

PLAYLISTS
---------
Use New playlist in the sidebar to open the playlist editor. You can choose the
playlist name and optionally select custom artwork. Select an existing playlist
and use Edit playlist to change its name or replace its artwork.

Playlist files use the .cmbpl extension. They store playlist metadata and track
references without duplicating the audio itself.

Playlist rows expose star, like, show-in-folder, and remove actions. Removing an
item from a playlist never deletes its Library file. User-created playlists have
separate Edit playlist and Delete playlist buttons; deleting a playlist also
leaves all of its media files intact.

DOWNLOADER
----------
Open Downloads from the sidebar.

- Paste a direct media URL or an explicit playlist page that yt-dlp can resolve.
- A song/watch URL containing playlist or auto-mix parameters downloads only that
  song. An explicit playlist page downloads the complete playlist.
- YouTube artist, channel, and browse pages are rejected to prevent accidental
  bulk catalog downloads.
- Choose MP3, M4A, OPUS, FLAC, WAV, Original audio, or MP4 video.
- MP4 downloads can be capped at 360p, 480p, 720p, 1080p, 1440p, or 2160p,
  or left at Best available.
- Downloads are placed directly into the Charter library.
- Up to 32 downloads can run concurrently. Adding another link while work is in
  progress queues another independent job, so whichever finishes fastest appears
  in the Library first.
- Active jobs appear as individual queue rows with their URL, selected format,
  current downloader status, and progress. The URL field clears after each job is
  accepted so another link can be pasted immediately.
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
The supplied icon pack is under `assets/icons`. Charter embeds and uses the
provided ICO files directly. Transport symbols that are not resource-backed are
drawn directly by the app at the active DPI.

KEYBOARD SHORTCUTS
------------------
Ctrl+1   Library
Ctrl+2   Downloads
Ctrl+3   Settings
Ctrl+F   Library search
Ctrl+D   Downloader URL field
Ctrl+O   Open Charter music folder
Ctrl+I   Import external audio or video
Delete   Permanently delete all selected Library items (with confirmation)
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
From the repository directory:

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
windres resources/CharterMediaPlayer.rc -I resources -O coff \
    -o build/CharterMediaPlayer_resources.o

gcc src/main.c build/CharterMediaPlayer_resources.o \
    -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic \
    -municode -mwindows \
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm \
    -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 \
    -o build/CharterMediaPlayer.exe

rm -f build/CharterMediaPlayer_resources.o
```

BUILDING FROM CMD
-----------------
Run `build.bat`.

The finished program is `build/CharterMediaPlayer.exe`. `build.bat` also detects
the default `C:\msys64\ucrt64\bin` installation when GCC is not already on `PATH`.

GITHUB
------

Generated files are excluded by `.gitignore`, text and binary attributes are
defined, and `.github/workflows/build.yml` builds and uploads the Windows
executable for pushes and pull requests.

No license has been selected for the project. Choose and add the license you want
before publishing if other people should be allowed to reuse or redistribute the
source.
