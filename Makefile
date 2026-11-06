CC ?= gcc
WINDRES ?= windres

TARGET := build/CharterMediaPlayer.exe
RESOURCE_OBJECT := build/CharterMediaPlayer_resources.o
SOURCE := src/main.c
GAPI_SOURCE := components/CharterGAPI/src/charter_gapi.c
GAPI_HEADER := components/CharterGAPI/include/charter_gapi.h
RESOURCE_SCRIPT := resources/CharterMediaPlayer.rc
ICON_RESOURCES := assets/CharterMediaPlayer.ico $(wildcard assets/icons/*.ico)

CFLAGS := -Isrc -Iresources -Icomponents/CharterGAPI/include -O2 -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS := -municode -mwindows
LDLIBS := -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm \
	-ldwmapi -luxtheme -lurlmon -lmfplat -lmfreadwrite -lmfplay \
	-lmfuuid -lpropsys -lmsimg32 -ladvapi32 -lcomdlg32

.PHONY: all clean

all: $(TARGET)

build:
	mkdir -p build

$(RESOURCE_OBJECT): $(RESOURCE_SCRIPT) resources/resource.h resources/CharterMediaPlayer.manifest $(ICON_RESOURCES) | build
	$(WINDRES) $(RESOURCE_SCRIPT) -I resources -O coff -o $@

$(TARGET): $(SOURCE) $(GAPI_SOURCE) $(GAPI_HEADER) $(wildcard src/*.inc.c) src/charter_internal.h $(RESOURCE_OBJECT) | build
	$(CC) $(SOURCE) $(GAPI_SOURCE) $(RESOURCE_OBJECT) $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf build
