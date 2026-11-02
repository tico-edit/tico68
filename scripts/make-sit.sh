#!/bin/sh
# Make tico68.sit, a classic StuffIt archive for a Macintosh with no way to
# read the disk image: the application, unpacked from Retro68's MacBinary so
# that its fork and Finder information are archived rather than the .bin, and
# the Read Me as make-disk.sh leaves it.  docs/DESIGN-NOTES.md says why this
# format, and why sit.
#
# Usage: scripts/make-sit.sh BUILD_DIR SIT

set -eu

BUILD=$1
SIT=$2
STAGE=$BUILD/sit
OUT=$BUILD/tico68.sit

# Every file carries this date, 1 January 2026, so the same input makes the
# same bytes.  sit reads it with strncpy, so it must have no zero bytes.
DATE='\345\173\151\200'

# sit's .info file: its struct, which is not quite MacBinary's layout.
# info FILE NAME TYPE CREATOR FLAGS
info() {
    {
        printf '\000\000'
        printf "\\$(printf %03o ${#2})"
        printf %s "$2"
        head -c $((63 - ${#2})) /dev/zero
        printf %s "$3$4"
        printf "$5"
        head -c 16 /dev/zero
        printf "$DATE$DATE"
    } > "$1"
}

# A big-endian 32-bit number at an offset in a file.
long() {
    od -An -tu1 -j"$2" -N4 "$1" |
        awk '{ print $1 * 16777216 + $2 * 65536 + $3 * 256 + $4 }'
}

BIN=$BUILD/tico68.bin
[ "$(long "$BIN" 83)" -eq 0 ] || {
    echo "$BIN has a data fork, which this doesn't expect" >&2
    exit 1
}
rlen=$(long "$BIN" 87)

rm -rf "$STAGE" "$OUT"
mkdir -p "$STAGE"

# sit needs the data fork's file even when it's empty
: > "$STAGE/tico68"
tail -c +129 "$BIN" | head -c "$rlen" > "$STAGE/tico68.rsrc"
# Retro68 leaves the Finder flags clear: hasBundle, or no icon
info "$STAGE/tico68.info" tico68 APPL Tc68 '\040\000'

cp "$BUILD/ReadMe.txt" "$STAGE/ReadMe.txt"
info "$STAGE/ReadMe.txt.info" ReadMe.txt TEXT Tc68 '\000\000'

"$SIT" -o "$OUT" "$STAGE/tico68" "$STAGE/ReadMe.txt"
echo "wrote $OUT"
