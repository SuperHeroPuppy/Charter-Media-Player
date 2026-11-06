# Charter Media Player for iOS

This branch starts the native iPhone/iPad app. It imports local audio and video
with the system document picker, copies those files into app storage, persists
the library, and plays them with AVPlayer. It has play/pause and previous/next
controls. Downloads, playlists, themes, and background playback are later work.

On a Mac with Xcode and [XcodeGen](https://github.com/yonaskolb/XcodeGen):

```sh
xcodegen generate
open CharterMediaPlayer.xcodeproj
```

Select a development team and run the `CharterMediaPlayer` target on a device
or simulator. The project targets iOS 16 or newer. Xcode and the iOS SDK are
unavailable on this Windows machine, so this branch has not been compiled yet.

Imported files are copied into the app's Application Support folder. iOS
distribution still requires Apple signing and a suitable distribution method.

Source: [iOS branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/ios).
