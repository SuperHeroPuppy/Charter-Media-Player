#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

echo "Building Charter Media Player..."

mkdir -p build
windres resources/CharterMediaPlayer.rc -I resources -O coff -o build/CharterMediaPlayer_resources.o

gcc src/main.c build/CharterMediaPlayer_resources.o \
    -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic -municode -mwindows \
    -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay -lmfuuid -lpropsys -lmsimg32 -ladvapi32 \
    -o build/CharterMediaPlayer.exe

rm -f build/CharterMediaPlayer_resources.o

echo
echo "Build complete: build/CharterMediaPlayer.exe"
