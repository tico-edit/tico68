# The Macintosh application, for Retro68's toolchain; the Makefile
# runs this.  Nothing here selects a CPU: Retro68's default is the 68000.
cmake_minimum_required(VERSION 3.13)
project(tico68 C)

file(STRINGS VERSION TICO68_VERSION LIMIT_COUNT 1)

add_application(tico68
    src/buffer.c src/text.c src/pool.c src/doc.c src/bindings.c
    src/bars.c src/prefs.c src/save.c
    src/mac/main.c src/mac/win.c src/mac/menus.c src/mac/files.c
    src/mac/prefsfile.c
    src/mac/tico68.r
    CREATOR Tc68
)

# Don't link with -gc-sections: on this toolchain it has produced an app
# that dies with "out of memory" at launch on a Plus under System 6.
target_compile_options(tico68 PRIVATE
    -Wall -Wno-multichar -Wno-pointer-sign -Wno-array-bounds -Os -DVERSION="${TICO68_VERSION}")
