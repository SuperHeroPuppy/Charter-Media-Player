CC ?= gcc
WINDRES ?= windres

TARGET := build/CharterMusicBrowser.exe
RESOURCE_OBJECT := build/CharterMusicBrowser_resources.o
SOURCE := src/main.c
RESOURCE_SCRIPT := resources/CharterMusicBrowser.rc
ICON_RESOURCES := assets/CharterMusicBrowser.ico $(wildcard assets/icons/*.ico)

CFLAGS := -Isrc -Iresources -O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS := -municode -mwindows
LDLIBS := -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm \
	-ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay \
	-lmfuuid -lpropsys -lmsimg32

.PHONY: all clean

all: $(TARGET)

build:
	mkdir -p build

$(RESOURCE_OBJECT): $(RESOURCE_SCRIPT) resources/resource.h resources/CharterMusicBrowser.manifest $(ICON_RESOURCES) | build
	$(WINDRES) $(RESOURCE_SCRIPT) -I resources -O coff -o $@

$(TARGET): $(SOURCE) $(wildcard src/*.inc.c) src/charter_internal.h $(RESOURCE_OBJECT) | build
	$(CC) $(SOURCE) $(RESOURCE_OBJECT) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf build
