# Answers to the design's open questions

Section 16 of [DESIGN.md](DESIGN.md) left these for implementation.

1. **Legacy and modern bindings.** Taken from tico's `src/keymap.rs`;
   see [KEYBINDINGS.md](KEYBINDINGS.md). ⌘ is Ctrl, ⌘-Option is Meta,
   Esc or Clear then a key is also Meta.
2. **Accelerator conflicts.** There are none to resolve, since keys never
   go through `MenuKey`: each profile's table decides, and the letters in
   the menus are labels set from that table.
3. **Automatic bars.** A bar shows at 32 rows and hides below 30, so a
   resize near the edge doesn't flicker. Monaco 9 on a 512 × 342 screen
   is well below that.
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
