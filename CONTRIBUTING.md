# Working on Charter

Choose the platform branch before changing application code. `main` contains
project information only; platform branches carry their own source, build
instructions, and release notes. Do not merge an entire platform branch into
`main` to publish a release.

- Windows changes: `windows`
- Android changes: `android`
- iOS changes: `ios`
- Linux changes: `linux`
- Shared Win32 graphical API: [CharterGAPI](https://github.com/SuperHeroPuppy/Charter-GAPI)

The release server is private and is not mirrored in this public repository.
Keep server credentials and release binaries out of commits.
