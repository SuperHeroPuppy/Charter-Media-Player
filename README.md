# Charter Media Player

A lightweight native Windows audio/video downloader, media library, playlist manager,
and dedicated in-app player written in C for Windows 10 and 11.

Current release: **R 1.0.1 (build 1022-M4A)**

Build identifiers use `sequence-feature`: the numeric sequence always increases,
while the short suffix records the focus of that build. `M4A` identifies the
audio format-change fix for M4A playback.

## Quick start

Install the MSYS2 UCRT64 GCC toolchain, then run:

```sh
bash build.sh
```

The executable is written to `build/CharterMediaPlayer.exe`.

### Windows publisher and release signing

The executable's Windows version information identifies its company as
**Super's Network**. This metadata appears in Explorer but does not by itself make
the executable trusted. Windows obtains the verified publisher shown by security
prompts from the executable's Authenticode signature.

For a public release, install a trusted code-signing certificate whose verified
subject is **Super's Network**, build the application, and run:

```powershell
$env:CHARTER_SIGNING_THUMBPRINT = "YOUR_CERTIFICATE_THUMBPRINT"
.\sign-release.bat
```

`sign-release.bat` signs with SHA-256, applies an RFC 3161 timestamp, and verifies
the result. It requires SignTool from the Windows SDK. Keep certificates and
credentials outside the repository, sign every released executable consistently,
and never modify the executable after signing it.

CURRENT INTERFACE
-----------------
Charter uses a music-service style layout with a permanent left sidebar and a
persistent player across the bottom of the window.

- Library, Downloads, Settings, and playlists live in the sidebar.
- The Library shows a dedicated artwork column, title with file size beneath it,
  artist/creator, and per-row actions for star, like, blacklist, playlist, folder,
  and delete.
- Ctrl-click selects multiple rows. Actions such as star, like, add to playlist,
  and delete apply to the full selection.
- Drag rows to save a custom Library or playlist order without losing the current
  scroll position or moved-row selection.
- Double-clicking audio starts playback inside Charter; video opens Charter's own video window.
- Import media copies one or more external audio/video files into the Charter library.
- Delete permanently removes every selected media file, its yt-dlp sidecar, and
  its references from Charter playlists after one confirmation.
- Starred and liked state survives refreshes. Starred media is highlighted and
  pinned above unstarred media. Likes fill the heart action and populate the
  built-in Liked playlist in the sidebar.
- Previous, play/pause, next, seek, and volume stay in the bottom player.
- Loop and the three-state Off/Shuffle/Smart Shuffle control stay beside the main
  transport controls. Smart Shuffle favors different creators and holds the
  previous creator out of automatic selection for five songs whenever the current
  library or playlist has enough variety.
- Volume sits beside the playback progress bar.
- Audio output selection lives on the Settings page instead of the player bar.
- Settings uses a compact two-column layout. Themes include the immutable
  original Charter look and user-created themes with a full color-and-font builder.
  Themes can be imported, exported, and refreshed without restarting.
- Optional Discord Rich Presence can share the current title, artist, media type,
  play/pause state, and playback timeline. Settings can use Charter's provided
  Application ID or a custom Discord Application ID.
- Optional experimental **Windows background listening** changes the close button
  into a hide-to-background action without interrupting playback. While hidden,
  Charter appears in the Windows notification area; left-click restores it and
  its dark native menu provides playback, Settings, and Exit actions.
- Launching Charter again restores the running instance instead of creating a
  second background icon. A media file opened through Windows is passed to that
  instance for playback.
- Optional experimental **Pinned background player** turns minimize into a compact,
  always-on-top player at the top of the active screen. With the option disabled,
  minimize remains invisible while preserving a compact frame for screen capture.
- Opening a supported media file with `CharterMediaPlayer.exe` starts the compact
  micro player without copying or adding that file to the Charter library. Videos
  continue into Charter's dedicated video window.
- Standard keyboard media controls are supported for play/pause, previous, next,
  stop, volume up/down, and mute.
- The download source selector updates its action immediately; switching to or
  from name search clears incompatible input. Escape clears the download field,
  and text match results appear before preview thumbnails finish loading.
- Clear search removes the current match cards and their previews, clears the
  input, and focuses it for another search.
- The Library's View queue button shows upcoming manual and automatic playback
  in place of the media list. Right-click a track to add it to the queue,
  put it next, or remove it from the current queue.
- Library and playlist rows support moving a multi-selection together. The
  Actions column remains anchored to the right edge while other columns resize.
- Optional experimental **Priority media keys** captures those standard keys
  while Charter is playing or paused, including when its window is minimized or
  hidden. For keys it receives, Charter suppresses delivery to other players;
  turn it off to restore normal Windows behavior. Nonstandard device controls
  may not generate these keys.
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

Starred, liked, and Library blacklist state is stored in Library State.cmbstate under
the Charter app-data folder. Per-playlist blacklists are stored independently in each
playlist file. The generated Liked.cmbpl remains compatible with the normal playlist
format.
Custom Library row order is stored in Library Order.cmborder.
Discord presence preferences are stored in Discord Presence.cfg in the same app-data
folder. The Application ID is public configuration data; Charter stores no Discord
login, user token, or OAuth secret.

Volume, audio-output choice, loop, shuffle mode, background listening, pinned
player, and priority media-key preferences are stored in Player Settings.cfg and
restored when Charter starts. All experimental options default to off. If a
saved output device is temporarily unavailable, Charter uses the system
default while retaining the saved device choice for a later refresh or restart.

Themes are stored as UTF-8 JSON in the active Charter app-data folder's `Themes`
subfolder (`%LOCALAPPDATA%\Charter Media Player\Themes` after migration).
`active.json` remembers the selection. Create a theme to reveal the Theme Builder,
where every application palette color and the body/display font families can be
edited. Each color swatch explains where it appears. In particular, Selected
media row colors selected library/playlist entries, Text on accent colors the
text/icons on accent-filled buttons, and Unfilled slider track is separate from
Strong outlines. Theme JSON filenames follow their theme names. The built-in Default
Charter palette cannot be changed. The Themes folder is watched for added or
changed JSON files, and Settings also has a manual Refresh button.
Legacy `custom-dark.json` and `custom-light.json` files are left untouched but
are no longer offered as built-in presets; they may be imported as user themes.
User-created themes can be deleted from Settings; Charter asks first and moves
the theme JSON to the Recycle Bin. Older theme JSON files without a slider-track
color continue to use their border color for the unfilled slider track.

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

MP3, WAV, WMA, AAC/M4A, FLAC and other Windows formats play directly. M4A
playback requests standard PCM output and handles format changes during decoding.
OGG, OPUS, AIFF, APE, WebM audio, and other imported/downloaded audio formats are normalized to
temporary PCM for playback when needed. Video includes MP4/M4V, MOV, WMV, AVI, MKV,
WebM, MPEG, OGV, FLV, TS and M2TS; actual external-video codec support depends on the
decoders installed in Windows. Charter's MP4 downloads request H.264/AAC for reliable
in-app playback.

Playback features include:

- Double-click to play.
- Previous and next.
- Stable shuffled queues: pressing Previous or Next walks actual playback history
  instead of reshuffling on every click. Changing shuffle mode resets both the
  upcoming queue and that mode's history.
- Smart Shuffle prioritizes different creators and avoids creators heard within the
  previous five songs whenever the available selection permits it.
- Persistent blacklist actions exclude media from automatic playback while leaving
  it available for manual double-click playback. Library and playlist blacklists are
  intentionally independent.
- Play/pause.
- Automatic next-track playback.
- Seeking with elapsed and total time.
- Per-app volume.
- Output-device selection on Settings.
- Full-screen video using the button, F11, Alt+Enter, or a double-click.
- Auto-hiding video controls that return when the pointer moves.
- A micro player for files opened directly from Explorer or the command line.
- Hardware multimedia-key support through the standard Windows media commands.

### Windows Default Apps

On Settings, choose **Choose defaults** under Windows integration. Charter registers
its current executable location and supported audio/video extensions for the current
Windows user, then opens the Windows Default Apps page. Windows requires the user to
make the final association choice. Register again if the portable executable is moved
to another folder.

DISCORD RICH PRESENCE
---------------------
Discord Rich Presence is optional and off by default. To enable it:

1. Open Settings and choose **Provided ID** to use Charter's bundled public
   Application ID (`1552645707192733697`), or choose **Custom ID** and paste your
   own numeric Discord Application ID.
2. Turn Discord presence on and choose **Save & connect**.

Charter talks only to the locally running Discord desktop app. It reconnects
automatically if Discord is opened later. Disabling the setting or closing Charter
clears its activity. Audio appears as **Listening**, with the song title, artist,
and a live progress bar when its duration is known. Video appears as **Watching**.
For Charter-downloaded YouTube media with a matching yt-dlp `.info.json` file,
Discord can show the video's public thumbnail. Local-only artwork cannot be sent
through Discord Rich Presence, so imported files use the Developer Portal
application icon. The application name comes from the selected Discord Application ID.

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
- Choose YouTube (the default), SoundCloud, Bandcamp, Vimeo, or Other supported
  site. Pasting a link from a different supported source switches the choice
  automatically. Actual availability depends on yt-dlp and the source site.
- A Spotify track link uses its public title preview to search YouTube for five
  likely matches. Review each thumbnail, title, and creator in the search pane
  beside the download queue, then click a match to download it. Album and
  playlist links are not expanded automatically.
- Amazon Music pages do not reliably expose the selected track name to Charter.
  Pasting an Amazon Music link switches to Search by name; enter the title and
  artist, then review the YouTube matches. Search by name also works directly.
- This match flow downloads the selected YouTube video, not audio from Spotify
  or Amazon Music. Purchased local music can still be imported normally.
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
|-- assets/              Application artwork and embedded ICO resources
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
    -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 -lcomdlg32 \
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
