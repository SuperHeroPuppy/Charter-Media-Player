# Charter Media Player for iOS

Use the [ios branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/ios)
for the source and build commands in this guide.

This branch starts the native iPhone/iPad app. It imports local audio and video
with the system document picker, copies those files into app storage, persists
the library, and plays them with AVPlayer. It has play/pause and previous/next
controls. Downloads, playlists, themes, and background playback are later work.
The UI compiles the first-party CharterGAPI C core and uses its palette and
responsive layout sizes through a Swift bridging header. Its source snapshot
lives in `components/CharterGAPI`.

On a Mac with Xcode and [XcodeGen](https://github.com/yonaskolb/XcodeGen):

```sh
xcodegen generate
open CharterMediaPlayer.xcodeproj
```

Select a development team and run the `CharterMediaPlayer` target on a device
or simulator. The project targets iOS 16 or newer. A free Apple Account can
sign for testing on your own iPhone through Xcode.

The [iOS build workflow](https://github.com/SuperHeroPuppy/Charter-Media-Player/actions/workflows/ios-build.yml)
also compiles the simulator and an unsigned iPhone IPA. Download the
`CharterMediaPlayer-ios-unsigned` artifact from a successful run. The IPA must
be signed before a phone will install it. On Windows, a third-party tool such
as [Sideloadly](https://sideloadly.io/) can sign and install it using your Apple
Account; follow that tool's device setup instructions. This path has not been
tested on every iPhone. The Mac/Xcode path above is Apple's supported option.

Imported files are copied into the app's Application Support folder. iOS
distribution still requires Apple signing and a suitable distribution method.

Source: [iOS branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/ios).
