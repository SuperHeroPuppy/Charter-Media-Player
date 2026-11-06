# Charter Media Player for Android

This branch starts the native Android app. It imports local audio and video
through Android's document picker, remembers access to selected files, and
plays them in a small library with play/pause, previous/next, and seek controls.
The UI uses the first-party CharterGAPI C core through JNI for its palette and
responsive layout sizes. Its source snapshot lives in `components/CharterGAPI`.
It does not yet include the Windows downloader, playlists, themes, background
audio service, or automatic updates.

Open the project in Android Studio with JDK 17+, Android SDK 35, NDK 27.2, and
CMake 3.22.1. It uses
Android Gradle Plugin 8.13.0 and Gradle 8.13, with no external app libraries.
Build the `app` module and install on Android 8.0 (API 26) or newer. This
Android branch CI builds a debug APK.

Imported files remain in their original location. If a document provider
revokes access or a file is moved, import it again. The app requests no broad
storage permission and no Internet permission.

Source: [Android branch](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/android).
