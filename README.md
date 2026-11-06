# Charter Media Player

Charter is a media player project with separate platform branches. This `main`
branch is the project guide. Platform documentation lives here; application
source and build scripts live in the branches below.

| Platform | Documentation | Source | Current state |
| --- | --- | --- | --- |
| Windows | [Windows guide](docs/WINDOWS.md) · [Architecture](docs/WINDOWS_ARCHITECTURE.md) | [windows branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/windows) | R 1.1.0 Call To ORDER, build 1030-GAPI |
| Android | [Android guide](docs/ANDROID.md) | [android branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/android) | Native local player starter; no APK release yet |
| iOS | [iOS guide](docs/IOS.md) | [ios branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/ios) | SwiftUI local player starter; no iOS release yet |
| Linux | [Linux guide](docs/LINUX.md) | [linux branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/linux) | GTK local player starter; no Linux release yet |

## Downloads and updates

The [download website](docs/PRIVATE_DOWNLOADS.md) and update service are
private. They require an account and are reachable only through an address
shared with authorized users. There is no public download-server URL in this
repository. The service hosts published build files and provides a release
manifest for future app updaters; automatic installation is still in progress.

## CharterGAPI

[CharterGAPI](https://github.com/SuperHeroPuppy/Charter-GAPI) is Charter's own
graphical API. Its portable C core supplies shared colors and layout sizes to
the Windows, Android, iOS, and Linux branches. Windows additionally uses its
Win32 drawing and control helpers. Each platform carries a **first-party**
source snapshot under `components/CharterGAPI`, so builds are self-contained.
The [separate repository](https://github.com/SuperHeroPuppy/Charter-GAPI#readme)
contains the API guide and lets other programs use it.

For contribution and branch guidance, see [CONTRIBUTING.md](CONTRIBUTING.md).
