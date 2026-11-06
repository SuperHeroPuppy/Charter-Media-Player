# Contributing to Charter

This is the single contribution guide for Charter Media Player. The `main`
branch contains project documentation. Make application changes on the matching
platform branch; do not merge an entire platform branch into `main`.

| Change | Branch | Guide |
| --- | --- | --- |
| Windows app | `windows` | [Windows](docs/WINDOWS.md) and [architecture](docs/WINDOWS_ARCHITECTURE.md) |
| Android app | `android` | [Android](docs/ANDROID.md) |
| iOS app | `ios` | [iOS](docs/IOS.md) |
| Linux app | `linux` | [Linux](docs/LINUX.md) |
| Shared graphics API | [CharterGAPI](https://github.com/SuperHeroPuppy/Charter-GAPI) | [API README](https://github.com/SuperHeroPuppy/Charter-GAPI#readme) |
| Project documentation | `main` | [Project overview](README.md) |

## Build and verify

Follow the platform guide above and run that branch's build. Keep changes
focused, compile with warnings enabled, and manually check the affected UI or
playback path when a device is available. Platform branches have CI builds.

Windows uses MSYS2 UCRT64 GCC. `scripts/build-program.bat` builds the program;
`build-all.bat` also builds the installer with Inno Setup 6. The Windows source
uses a unity build: `src/main.c` includes the `src/*.inc.c` fragments. Read the
[architecture guide](docs/WINDOWS_ARCHITECTURE.md) before moving those modules.

Update CharterGAPI in its own repository first, then synchronize the source
snapshots in each platform branch and run their build checks. The portable core
is shared across Windows, Android, iOS, and Linux; Win32 drawing helpers are
Windows-specific.

The release server is private and is not mirrored in this public repository.
Keep server credentials and release binaries out of commits.
