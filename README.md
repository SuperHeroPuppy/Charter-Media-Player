# Charter Media Player for Linux

This branch is the Linux application. The first milestone opens local audio
and video files, keeps an imported library under the user's XDG data folder,
and plays media when a library row is double-clicked, using GTK's native media
controls. It does not yet
include Charter's Windows downloader, playlists, themes, or updater.
The app compiles the first-party CharterGAPI C core and uses its palette and
layout sizes. Its source snapshot lives in `components/CharterGAPI`.

Install GTK 4.12+ development files and a GTK media backend (usually GStreamer),
then run:

```sh
make
./build/charter-media-player
```

GTK's available codecs depend on the installed media backend. The app does not
copy imported files; it remembers their URIs, so moving those files can break
the library entry. `make` needs `pkg-config`, a C compiler, and GTK 4 headers.

This source is on the `linux` branch of
[Charter Media Player](https://github.com/SuperHeroPuppy/Charter-Media-Player/tree/linux).
