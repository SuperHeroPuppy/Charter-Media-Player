# CharterGAPI

A first-party C library shared by Charter Media Player on Windows, Android,
iOS, and Linux. Its portable core can also be used by other programs. Windows
additionally has Win32 drawing and control helpers.

## Features

- `charter_gapi_core.h` provides a shared color palette, color blending, and
  responsive layout sizes without operating system dependencies.
- Native platform adapters convert the portable colors and sizes to Win32,
  Android, SwiftUI, and GTK presentation values.
- `cgapi_layout_begin/move/end` batches child-window moves so a resize appears
  as one layout change. Unchanged controls are skipped.
- `cgapi_set_visible` avoids repeated show/hide calls.
- `cgapi_set_text` changes a caption only when needed and paints the change
  before returning.
- `cgapi_paint_begin/dc/end` draws into an offscreen bitmap and presents the
  finished frame in one copy. `cgapi_redraw` can refresh a window and its
  children on demand.
- Rounded fills and outlines, lines, and styled text are available as drawing
  functions for any GDI device context.

## Build and use

On any platform, compile `src/charter_gapi_core.c` with C11 and include
`include/charter_gapi_core.h`, or use CMake to build `CharterGAPICore`.
`ctest` runs the portable core smoke test. For Windows, run `build.bat` with
MSYS2 UCRT64 GCC to create `build/libCharterGAPI.a`, including the core.
Run `build-demo.bat` to compile the independent example program.
Run `test.bat` for the portable and Win32 smoke tests.
Add `include` to your compiler's include path, include `charter_gapi.h`, and
link the library with `gdi32` and `user32`. The API accepts ordinary Win32
`HWND`, `HDC`, and pixel coordinates.

```c
CGapiLayout layout;
cgapi_layout_begin(&layout, parent, 2);
cgapi_layout_move(&layout, button, 16, 16, 120, 36);
cgapi_layout_move(&layout, edit, 144, 16, 300, 36);
cgapi_layout_end(&layout);
cgapi_set_text(button, L"Search");
```

The library does not create controls or own application state. Platform apps
carry a source snapshot under `components/CharterGAPI` to keep builds
self-contained. Changes to shared colors and metrics belong here first, then
the snapshots should be synchronized.
