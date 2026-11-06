# Charter Media Player for Android

Use the [android branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/android)
for the source and build commands in this guide.

This branch contains the native Android app. It imports local audio and video
through Android's document picker, remembers access to selected files, and
plays them in a searchable library with play/pause, previous/next, and seek
controls. Long-press a library item to play it next or add it to the queue;
the Queue button shows the active order. Long-press a queue item to remove it.
Audio continues in the background with Android system media controls. Opening
a media file from Android's Files app plays it without importing it.
The UI uses the first-party CharterGAPI C core through JNI for its palette and
responsive layout sizes. Its source snapshot lives in `components/CharterGAPI`.
It does not yet include the Windows downloader, playlists, themes, or automatic
updates.

Open the project in Android Studio with JDK 17+, Android SDK 36, NDK 27.2, and
CMake 3.22.1. It uses
Android Gradle Plugin 8.13.0, Gradle 8.13, and AndroidX Media3 1.11.1.
Build the `app` module and install on Android 8.0 (API 26) or newer. The
Android branch CI builds a debug APK.

Imported files remain in their original location. If a document provider
revokes access or a file is moved, import it again. The app requests no broad
storage permission and no Internet permission.

Source: [Android branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/android).
