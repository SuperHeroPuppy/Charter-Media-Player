# CharterGAPI

A small C library for responsive native Win32 interfaces. It is independent of
Charter Media Player and can be linked into other Windows programs.

## Features

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

Run `build.bat` with MSYS2 UCRT64 GCC to create `build/libCharterGAPI.a`.
Run `build-demo.bat` to compile the independent example program.
Run `test.bat` for a small layout, visibility, and caption smoke test.
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

The library does not create controls or own application state. Its source and
header can also be compiled directly with another program. Charter Media
Player carries a first-party snapshot under `components/CharterGAPI` so its builds remain
self-contained; update that snapshot when releasing a new CharterGAPI version.
