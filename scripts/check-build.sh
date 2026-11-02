#!/bin/sh
# Check what was built: the disk image, the application's Finder
# information, and that the code is plain 68000.
#
# Usage: scripts/check-build.sh BUILD_DIR [OBJECT]
#
# OBJECT, instead of the application, is for checking the check.

set -eu

BUILD=$1
IMAGE=$BUILD/tico68-800k.dsk
: "${RETRO68:=$HOME/opt/retro68/git/toolchain}"
PATH=$RETRO68/bin:$PATH

fail=0
bad() {
    echo "FAIL: $*"
    fail=1
}

size=$(wc -c < "$IMAGE")
[ "$size" -eq 819200 ] || bad "$IMAGE is $size bytes, not 819200"

# hfsutils keep their state in $HOME/.hcwd
mkdir -p "$BUILD/hfs"
hfs() {
    HOME=$BUILD/hfs "$@"
}

hfs hmount "$IMAGE" >/dev/null
listing=$(hfs hls -l)
hfs hcopy -r ":Read Me" "$BUILD/ReadMe.check" || bad "can't read Read Me"
hfs humount

echo "$listing" | grep -q 'APPL/Tc68.* tico68$' ||
    bad "no application tico68 of type APPL, creator Tc68"
echo "$listing" | grep -q 'TEXT/Tc68.* Read Me$' ||
    bad "no Read Me"

# The vers resource is written out by hand, so it can be left behind when
# VERSION changes.
version=$(cat VERSION)
vers="0x0${version%.*}, 0x${version#*.}, development, 0x00, verUS, \"$version\", \"$version, tico68\""
grep -qF "$vers" src/mac/tico68.r ||
    bad "the vers resource in src/mac/tico68.r doesn't match VERSION $version"

# tico68 shows the Read Me as it is, so: CR line endings, plain ASCII so that
# nothing depends on a font's high characters, and short enough for a window,
# since lines don't wrap.
[ "$(tr -dc '\n' < "$BUILD/ReadMe.check" | wc -c)" -eq 0 ] ||
    bad "Read Me has LF line endings"
tr '\r' '\n' < "$BUILD/ReadMe.check" > "$BUILD/ReadMe.lines"
! LC_ALL=C grep -q '[^ -~]' "$BUILD/ReadMe.lines" ||
    bad "Read Me has something other than printable ASCII"
! grep -q '.\{73\}' "$BUILD/ReadMe.lines" ||
    bad "Read Me has a line over 72 columns"
! grep -q '@VERSION@' "$BUILD/ReadMe.lines" ||
    bad "Read Me still has @VERSION@"

# Anything a 68020 or later has that a 68000 doesn't. An emulator set to
# a later model would run these, and a Plus would not.  Scaled index
# registers would belong here too, but strings in the code segment
# disassemble into those; code built for a 68020 shows these as well.
later='^(extbl|mulsl|mulul|divsl|divul|divsll|divull|bf[a-z]+|rtd|casw|casl|cas2[wl]|chk2[bwl]|cmp2[bwl]|pack|unpk|movec|moves[bwl]|b[a-z][a-z]?l)( |$)'
found=$(m68k-apple-macos-objdump -d "${2:-$BUILD/tico68.code.bin.gdb}" |
        cut -f3 | grep -E "$later" || true)
if [ -n "$found" ]; then
    echo "$found" | head -20
    bad "instructions a 68000 doesn't have"
fi

[ "$fail" -eq 0 ] && echo "build checks passed"
exit "$fail"
