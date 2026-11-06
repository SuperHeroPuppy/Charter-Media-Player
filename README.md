# Charter Media Player

Charter is a media player project with separate platform branches. This `main`
branch is the project guide; application source and build instructions live in
the branches below.

| Platform | Source and build instructions | Current state |
| --- | --- | --- |
| Windows | [windows branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/windows) | R 1.1.0 Call To ORDER, build 1029-PLAT |
| Android | [android branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/android) | Native local player starter; no APK release yet |
| iOS | [ios branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/ios) | SwiftUI local player starter; no iOS release yet |
| Linux | [linux branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/linux) | GTK local player starter; no Linux release yet |

## Downloads and updates

The [download website](docs/PRIVATE_DOWNLOADS.md) and update service are
private. They require an account and are reachable only through an address
shared with authorized users. There is no public download-server URL in this
repository. The service hosts published build files and provides a release
manifest for future app updaters; automatic installation is still in progress.

## CharterGAPI

[CharterGAPI](https://github.com/SuperHeroPuppy/Charter-GAPI) is Charter's own
reusable Win32 graphical API. The Windows branch carries it as a **first-party**
component under `components/CharterGAPI`; it is also maintained separately so
other programs can use it.

For contribution and branch guidance, see [CONTRIBUTING.md](CONTRIBUTING.md).
