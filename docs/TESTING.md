# Testing tico68

## On the host

`make unit` builds the portable code with the host compiler, and
`test_core` with ASan and UBSan.

- `tests/unit/test_text.c`: line endings in both directions, NUL and
  Ctrl+Z, long lines, and a pseudo-random comparison of whole-buffer
  loading with byte-at-a-time loading.
- `tests/unit/test_core.c`: editing, movement, selection, per-document
  format, all three key tables including legacy ⌘Y and ⌘V, the bars,
  preferences, safe-save order with failures at each step, and running
  out of memory.

`make test` also runs `scripts/check-build.sh`: the image is 800K, the
application is `APPL`/`Tc68`, and the disassembly has no 68020
instructions.

## Under Mini vMac

```
python3 tests/emu/run.py --system 6 --keep DIR
python3 tests/emu/run.py --system 7 --keep DIR
```

`make emu` runs it for System 7. It needs Mini vMac built as a Plus with
4 MB, with `minivmac` and `vMac.ROM` in `~/opt/minivmac/git/run`, Xvfb,
xdotool, hfsutils, Python with Pillow, and a bootable HFS image with
`:System Folder` for System 6 and `:System Folder 7.0.1` for System 7.
`BOOT_DSK` names the image; only a copy of it is used. Screenshots land
in `DIR`.

Mini vMac runs on its own Xvfb display. SDL needs `WAYLAND_DISPLAY`
unset and `SDL_VIDEODRIVER=x11` or the window opens on the real desktop;
`tests/emu/macemu.py` sees to that. Super is ⌘ and Alt is Option. The
checks read the data disk with hfsutils after the run, so they are about
the bytes and Finder information actually written, except the caret's,
which read the screen during it:

- edits to CR files, and CR kept; a CRLF file converted to LF without
  touching another open document;
- a resource fork, type and creator kept through a save;
- a new file's type, creator and bytes; a locked file refused and the
  text kept;
- legacy ⌘V and ⌘Y paging, Meta by ⌘-Option and Esc, Preferences kept
  across launches, modern menu labels;
- the mouse: drag, double-click, cut and paste, a clipboard paste into
  another document, resizing, Window menu, Font menu;
- the caret left behind by no move, across its blink, and not drawn
  while it's scrolled out of view;
- no temporary files left.

Run by hand, not in `run.py`: a document double-clicked in the Finder
(System 6 and 7), and desk accessories from the Apple menu.

## Measured

Mini vMac, Plus, 4 MB, System 6, no MultiFinder, at its 2× setting, with
lines of 25 characters. A Plus would be about half this speed, and
emulated disks have no seek or transfer time, so these are the best case
for a real one.

| File | Open | Memory |
| --- | --- | --- |
| 100 KB, 3,800 lines | 3.3 s | 197 KB |
| 415 KB, 15,200 lines | 13 s | not separately measured |
| 1.58 MB, 57,000 lines | 51 s | 3.18 MB, leaving 711 KB free |
| 3.2 MB, 114,000 lines | runs out | alert; the application carries on |

Typing kept up in the 1.58 MB document, and saving it worked. With one
empty window, about 3.89 MB of the machine's 4 MB was still free in the
application's heap. `plat_alloc` also keeps back `RESERVE`, 96,000 bytes,
for the Toolbox. The smallest partition that works, including the
512 KB minimum in the `SIZE` resource, has not been measured.

## Not tested yet

A first run on wombat went well, but the acceptance list below hasn't
been worked through, and nothing has run on any other real Mac. Under
Mini vMac the screen is 512 × 342, so:

- bars on Auto in a large window have only been tested in the host code;
  Show and Hide have been seen on screen, and Auto hiding them;
- nothing here says how fast a Plus really is, or how it behaves with
  MultiFinder, and memory limits on System 7 have not been measured;
- the Option-arrow keys and the Esc-then-key form of every binding are
  covered by the key tables' tests, not all on screen.

## On hardware, for acceptance

The design's section 13 has the list. On wombat, under System 6 and
7.0.1, from the 800K image:

1. Launch and quit from the floppy, and from the BlueSCSI volume.
2. Open a CR, a CRLF and an LF file; edit, save, and read them back on
   the machine they came from.
3. Save a file with a resource fork on the BlueSCSI volume; check it in
   ResEdit.
4. Fill a floppy, then Save, and see the alert with the text still there.
5. Type quickly in a 100 KB file and scroll through it.
6. Open as big a file as you actually use and time it.

On chimera, or another later Mac: larger windows and bars on Auto, the
clipboard with another application, 68k emulation if it's a PowerPC.
