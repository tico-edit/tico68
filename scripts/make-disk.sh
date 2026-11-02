#!/bin/sh
# Make the 800K HFS disk image for Floppy Emu: the application, with its
# resource fork and Finder information, and docs/ReadMe.txt as a document
# that opens in it.
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
sed "s/@VERSION@/$(cat VERSION)/" docs/ReadMe.txt | tr '\n' '\r' > "$BUILD/ReadMe.txt"
hfs hcopy -r "$BUILD/ReadMe.txt" ":Read Me"
hfs hattrib -t TEXT -c Tc68 ":Read Me"
hfs humount
# Retro68's own image has only the application, and is too easily mistaken
# for this one
rm -f "$BUILD/tico68.dsk"
echo "wrote $OUT"
