CC ?= cc
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic $(shell pkg-config --cflags gtk4)
LDLIBS += $(shell pkg-config --libs gtk4)

.PHONY: all
all: build/charter-media-player

build/charter-media-player: src/main.c
	mkdir -p build
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@
