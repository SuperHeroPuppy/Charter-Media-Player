CC ?= cc
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic -Icomponents/CharterGAPI/include $(shell pkg-config --cflags gtk4)
LDLIBS += $(shell pkg-config --libs gtk4)

.PHONY: all
all: build/charter-media-player

build/charter-media-player: src/main.c components/CharterGAPI/src/charter_gapi_core.c components/CharterGAPI/include/charter_gapi_core.h
	mkdir -p build
	$(CC) $(CFLAGS) src/main.c components/CharterGAPI/src/charter_gapi_core.c $(LDLIBS) -o $@
