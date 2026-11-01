# Build tico68, a classic Macintosh application, with Retro68.
#   make            the application and an 800K disk image, in build/mac
#   make unit       the portable code's tests, on the host
#   make test       unit, then the build's checks
#   make emu        drive the application under Mini vMac (see docs/TESTING.md)
#
# RETRO68 is the toolchain directory, holding bin/ and m68k-apple-macos/.

RETRO68 ?= $(HOME)/opt/retro68/git/toolchain
TOOLCHAIN = $(RETRO68)/m68k-apple-macos/cmake/retro68.toolchain.cmake

VERSION := $(shell cat VERSION)

HOSTCC     = cc
HOSTCFLAGS = -std=c89 -pedantic -Wall -Wextra -Werror -O1 -g \
             '-DVERSION="$(VERSION)"'

CORE    = src/buffer.c src/text.c src/pool.c src/doc.c src/bindings.c \
          src/bars.c src/prefs.c src/save.c
HEADERS = src/tico.h

all: build/mac/tico68-800k.dsk

build/mac/Makefile: CMakeLists.txt
	cmake -S . -B build/mac -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN)

build/mac/tico68-800k.dsk: build/mac/Makefile FORCE
	cmake --build build/mac
	PATH=$(RETRO68)/bin:$$PATH sh scripts/make-disk.sh build/mac

build/test_text: tests/unit/test_text.c src/buffer.c src/text.c src/pool.c $(HEADERS)
	mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -o $@ tests/unit/test_text.c src/buffer.c src/text.c src/pool.c

build/test_core: tests/unit/test_core.c $(CORE) $(HEADERS)
	mkdir -p build
	$(HOSTCC) $(HOSTCFLAGS) -fsanitize=address,undefined -o $@ tests/unit/test_core.c $(CORE)

unit: build/test_text build/test_core
	./build/test_text
	./build/test_core

test: unit all
	RETRO68=$(RETRO68) sh scripts/check-build.sh build/mac

emu: all
	python3 tests/emu/run.py

clean:
	rm -rf build

.PHONY: all unit test emu clean FORCE
FORCE:
