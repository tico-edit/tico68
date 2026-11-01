# Known differences from tico

tico68 follows [tico](https://github.com/tico-edit/tico), and through it
GNU nano 8.7.1, for the editing it implements, and tico16 for the
portable code. This file records the places where it knowingly does
not. When a difference is removed, delete its entry.

## Most commands are not there

tico68 has movement, typing, the mark, line cut/copy/paste, Location,
Read File, Write Out, Save and Exit. Help, search and replace, undo, Go
To Line, Execute, justify, the toggles and the rest are left out, and
their keys report `Unbound key: KEY`. The shortcut bar lists only the
commands that exist.

## Windows, not buffers

Each document is a window, so Read File (`^R`, or `^O` in the modern
profile) opens one rather than inserting into the current text. `M-<`
and `M->` move between windows. `^X` closes the front window, and quits
after the last one. The menus always work, in every profile.

## New documents are saved in Macintosh format

tico writes a buffer with no line break in it in Unix format and tico16
in DOS format. tico68 uses CR. Files with line breaks keep what the
first one shows, as in tico.

## Format is a menu, not a prompt

The Write Out prompt's format toggles are the checked items in the File
menu, applying to the front document on its next save. They are inline
in the File menu rather than the hierarchical File → Text Format of the
design, because a Plus ROM doesn't have dependable hierarchical menus.

## Prompts are dialogs

Write Out is Standard File's Save dialog, and "Save modified buffer?"
is a Save / Don't Save / Cancel alert. Errors are alerts and don't lose
the text.

## Selections

The mouse and Shift-movement make a selection as on a Mac. tico's mark
(`M-A`, `^6`) sets the same kind of selection. Typing replaces it,
including one made with the mark, which nano doesn't do. Scrap and cut
buffer are separate: `^K` and `^U` use tico's cut buffer, shared by all
documents, and Cut, Copy and Paste use the Mac clipboard.

## ⌘ is Ctrl, ⌘-Option is Meta

The Plus has no Control key. Plain Option is left to type MacRoman
characters, so it can't be Meta. Esc, or Clear on a Plus, then a key is
also Meta, as in a terminal, but not in the Macintosh profile.

## Bytes from 128 up are MacRoman

There is no UTF-8. A byte from 128 to 255 is drawn as whatever MacRoman
glyph it is, and saved unchanged. A DOS or Unix file's accented letters
look wrong but survive. A column is a byte. Control characters are shown
as `^X`, in two columns, as in tico.

## Horizontal scrolling

A long line doesn't scroll by itself with `<` and `>` marks, as nano's
do. The whole window scrolls sideways, with the horizontal scroll bar,
as the cursor moves past the edge.

## The bars and the status line

The two bars are optional, and hidden by default on a small screen: see
Preferences in the README. nano's status line is the strip beside the
horizontal scroll bar. Bar text is always Monaco 9, with a ⌘ drawn in
place of `^`.

## A trailing Ctrl+Z is kept, out of sight

As in tico16.

## Saving

The text is written to `name.tmp` in the same folder, the original is
grown if it needs to be, then the original's data fork is overwritten
and the temporary file deleted. That keeps its other fork and Finder
information. If something stops it halfway, the temporary file holds
the text and tico68 says so.

Save As over another file replaces that file, forks included. The old
file is renamed to `name.bak` until the new one has its name, and goes
back if that fails. Any file left behind by a failure is named in an
alert.

## Long loads can't be interrupted

Keys typed during a long open may be lost when the event queue fills.

## System 7 Apple events are not handled

tico68 gets documents from the Finder the old way, which works under
both systems. It doesn't answer Apple events, so "Quit" from another
application won't work.
