# Contributing

Thanks for helping improve Charter Music Browser.

## Development setup

Install MSYS2 and use its UCRT64 environment with the GCC toolchain. From the
repository root, run one of:

```sh
bash build.sh
```

```sh
make
```

On Windows Command Prompt, `build.bat` provides the same build.

The resulting executable is `build/CharterMusicBrowser.exe`.

## Code organization

Read `docs/ARCHITECTURE.md` before moving functions between modules. The project
uses a deliberate unity build, so implementation fragments are included by
`src/main.c` and must not be compiled independently.

Please keep changes focused, compile with warnings enabled, and manually verify
the affected UI or playback path before opening a pull request.
