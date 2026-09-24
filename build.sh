#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

echo "Building Charter Music Browser..."

mkdir -p build
windres resources/CharterMusicBrowser.rc -I resources -O coff -o build/CharterMusicBrowser_resources.o

gcc src/main.c build/CharterMusicBrowser_resources.o \
    -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic -municode -mwindows \
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 \
    -o build/CharterMusicBrowser.exe

rm -f build/CharterMusicBrowser_resources.o

echo
echo "Build complete: build/CharterMusicBrowser.exe"
