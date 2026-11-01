#!/bin/sh
# Make the 800K HFS disk image for Floppy Emu: the application, with its
# resource fork and Finder information, and the README as a text file.
#
# Usage: scripts/make-disk.sh BUILD_DIR

set -eu

BUILD=$1
OUT=$BUILD/tico68-800k.dsk

# hfsutils keep their state in $HOME/.hcwd
mkdir -p "$BUILD/hfs"
hfs() {
    HOME=$BUILD/hfs "$@"
}

dd if=/dev/zero of="$OUT" bs=1024 count=800 status=none
hfs hformat -l "tico68" "$OUT" >/dev/null
hfs hcopy -m "$BUILD/tico68.bin" :tico68
tr '\n' '\r' < README.md > "$BUILD/README.txt"
hfs hcopy -r "$BUILD/README.txt" ":Read Me"
hfs hattrib -t TEXT -c Tc68 ":Read Me"
hfs humount
echo "wrote $OUT"
