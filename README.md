# tico68

tico68 is [tico](https://github.com/tico-edit/tico), by way of tico16,
as a native classic Macintosh application: a small text editor with a
window per document, tico's keys if you want them, and nothing on screen
that a 512 × 342 Macintosh Plus can't spare.

It is one 68000 binary, meant for a Macintosh Plus with 4 MB under
System 6 or System 7.0.1, and for later Macs running 68k code.

This is a first version. It is tested under Mini vMac (a Plus with 4 MB,
on System 6 and 7.0.1) and has had a first, good run on a real Macintosh
Plus. See [docs/TESTING.md](docs/TESTING.md).

## Using it

Double-click tico68, or a document in the Finder. Each document has its
own window, title and scroll bars; there are no tabs or panes.

**Files.** Open reads any file: if it isn't type `TEXT` you're asked
first, and only its data fork is edited. Save keeps a file's resource
fork, type, creator and Finder information. New files are `TEXT` with
creator `Tc68`. A failed save leaves your text in its window.

**Line endings.** A file keeps the line endings it came with: Macintosh
(CR), DOS (CRLF) or Unix (LF). The format for the front document is
checked in the File menu, and choosing another one takes effect for that
document on its next save. New documents are Macintosh. Mixed files
follow nano's rules, in `src/text.c`.

**Selecting.** Drag with the mouse, double-click for a word, or hold
Shift with an arrow key. Typing replaces the selection. The Mac
clipboard is used by Cut, Copy and Paste; Cut Line, Copy Line and Paste
Line use tico's separate cut buffer, which all documents share.

**The status strip.** The bottom row, beside the horizontal scroll bar,
shows messages, or else the document's format and "Modified".

**The menus** work the same in every keyboard profile: File, Edit, Go,
Font and Window. The ⌘ keys beside the items change with the profile.

## Keys

Edit → Preferences… chooses one of three keyboard profiles:

| Profile | What it is |
| --- | --- |
| Macintosh | The default: ⌘N, ⌘O, ⌘W, ⌘S, ⌘Q, ⌘X, ⌘C, ⌘V, ⌘A |
| Tico legacy | tico's nano-style keys: ⌘ is Ctrl, so ⌘Y and ⌘V page up and down |
| Tico modern | tico's modern keys: ⌘S, ⌘Q, ⌘O, ⌘X, ⌘C, ⌘V and so on |

⌘ is tico's Ctrl. Meta is ⌘-Option, or in the tico profiles Esc (Clear
on a Plus keypad) followed by a key. Plain Option types MacRoman
characters. The Plus has no Home, End or Page keys, so Option with the
arrows does those; the Go menu has them too.

All the tables are in [docs/KEYBINDINGS.md](docs/KEYBINDINGS.md).

## Preferences

Edit → Preferences… also sets tico's title bar and shortcut bar,
separately, to Always show or Always hide. Both show by default; hiding
them gives their rows to the text, which counts on a 512 × 342 screen.
The Font menu lists the fixed-width fonts and sizes; the default is
Monaco 9.

Settings are in a small text file, `tico68 Prefs`, in the Preferences
folder, or the System Folder under System 6:

```
profile=mac
title=show
shortcuts=show
font=Monaco
size=9
```

A missing or damaged file just gives the defaults.

## Limits

- Text is kept in memory, at 24 bytes for each line plus the line rounded
  up to 8 bytes. A file of short lines takes about twice its size.
- On a 4 MB Plus under System 6 without MultiFinder that means about a
  2 MB file at most: 1.6 MB opened with 700 KB to spare, and 3.2 MB
  failed cleanly with an alert. Under System 7 or MultiFinder the
  application gets the 2 MB partition in its `SIZE` resource, so expect
  about 1 MB; Get Info can raise that.
- Saving needs room on the disk for a second copy of the file for a
  moment.
- Opening is slow for big files: about 15 KB a second is the estimate
  for a Plus, doubled from measurements at 2× under Mini vMac.
  [docs/TESTING.md](docs/TESTING.md) has the figures.

Search, undo, syntax highlighting and much else are left out for now.
[DIFFERENCES.md](DIFFERENCES.md) has the details.

## Building

You need [Retro68](https://github.com/autc04/Retro68), CMake, and a C
compiler for the host tests.

```
make                     # build/mac/tico68-800k.dsk, plus tico68.bin
make unit                # the portable code's tests, on the host
make test                # those, then checks on the build
make emu                 # drive the application under Mini vMac
make RETRO68=/path/to/toolchain
```

`RETRO68` defaults to `~/opt/retro68/git/toolchain`. The 800K image is
plain HFS, for a Floppy Emu or an emulator: it holds the application and
`docs/ReadMe.txt` as a document that opens in it. That file is for reading
in tico68, so it is plain ASCII of at most 72 columns; when the keys,
preferences or limits here change, change it too.

To release, change `VERSION` in a PR. When it merges, the `release`
workflow tags the merge commit `vVERSION` and publishes a release with
the disk image attached.

## Source

| Where | What |
| --- | --- |
| `src/buffer.c`, `src/text.c` | Lines and line endings, from tico16 |
| `src/pool.c` | Allocator for lines, since Retro68's malloc is `NewPtr` |
| `src/doc.c` | The document: editing, movement, marks, status |
| `src/bindings.c` | Keys to commands, one table per profile |
| `src/bars.c` | tico's title and shortcut bars |
| `src/prefs.c` | The preferences file's contents |
| `src/save.c` | The order of steps in a safe save |
| `src/mac/` | Toolbox code: events, windows, menus, files, resources |
| `tests/unit/` | Host tests of everything above `src/mac/` |
| `tests/emu/` | The application driven under Mini vMac |

## License

[LICENSE](LICENSE), as for tico16.

The application icon is Tico the Toucan, by Pretianahart, redrawn in
black and white from the art on [ticoedit.org](https://ticoedit.org/mascot.html).
