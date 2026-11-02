# Answers to the design's open questions

Section 16 of [DESIGN.md](DESIGN.md) left these for implementation.

1. **Legacy and modern bindings.** Taken from tico's `src/keymap.rs`;
   see [KEYBINDINGS.md](KEYBINDINGS.md). ⌘ is Ctrl, ⌘-Option is Meta,
   Esc or Clear then a key is also Meta.
2. **Accelerator conflicts.** There are none to resolve, since keys never
   go through `MenuKey`: each profile's table decides, and the letters in
   the menus are labels set from that table.
3. **Automatic bars.** Dropped. Each bar is shown or hidden by its
   preference alone, whatever the window's size, and both show by
   default.
4. **Showing the format.** A checked item in the File menu, and in the
   status strip beside the horizontal scroll bar, which costs no row.
5. **Safe save.** Write `name.tmp`, grow the original, overwrite its data
   fork in place, delete the temporary file. The original keeps its
   resource fork and Finder information because it is the same file. It
   needs room for a second copy for a moment, and the original's data is
   at risk only during the overwrite, when the temporary file holds the
   whole text. The save order is in `src/save.c`.
6. **MacRoman and raw bytes.** Bytes are kept as they are and shown as
   MacRoman. Nothing is transcoded.
7. **Sharing source with tico16.** Copied for now: `src/buffer.c` and
   `src/text.c` began as tico16's and have since changed for the pool
   allocator and for loading speed. Sharing is worth another look when
   both have settled.
8. **Memory.** About twice a file's size for short lines, which makes
   about 2 MB the largest file on a 4 MB Plus under System 6. The
   smallest usable partition is still unmeasured; see
   [TESTING.md](TESTING.md) and the README's limits.

## The StuffIt archive

`tico68.sit` is for a Macintosh with FTP and no way to read the disk
image, so what matters most is what can expand it on a Plus. Two tools
can write StuffIt archives from Linux:

| | [sit](https://github.com/thecloudexpanse/sit) | [stuffit-rs](https://github.com/benletchford/stuffit-rs) 0.4.0 |
| --- | --- | --- |
| Writes | StuffIt 1.5.1, `SIT!`/`rLau` | Only the 1997 format; reads `SIT!` |
| Compression | LZW | None, method 13 or 14 |
| Expander 4.0.1, Plus | Expands; tested | "Corrupted or may not be a valid archive" |
| Forks on Linux | Sidecar files: `.rsrc`, `.info` | Library yes; the command line only on macOS |
| License | BSD 2-Clause | MIT or Apache-2.0 |
| Pinning | A commit: no releases or tags | Tagged, on crates.io, with a lockfile |
| Builds | C; needs `-D_GNU_SOURCE` on the CI compiler | Rust; cleanly |

stuffit-rs is the larger and better-kept project, but what it writes
doesn't open on the machine this is for, so `sit` it is. It's pinned by
commit in the `Makefile`, the only pin it offers. The tradeoffs: LZW
isn't the tightest compression (54,582 bytes of application go to about
41,000), and it costs a network fetch, which is why `make sit` is apart
from `make`, so that nothing about the disk image depends on it.

Things that `scripts/make-sit.sh` works around, none of them documented:

- The `.info` file follows sit's struct, whose comments give MacBinary's
  offsets, but `type` is really at byte 66.
- A file with only a resource fork still needs an empty data file.
- `.info` fields are copied with `strncpy`, so nothing survives past a
  zero byte in a date. Dates come from `.info` because otherwise they're
  the build's, and differ every time; a fixed date with no zero bytes
  makes the archive the same from one build to the next.
- `.rsrc` is compressed, where the AppleDouble route stores the fork as
  it is.
- Retro68 leaves the Finder flags clear, so the script sets `hasBundle`.
