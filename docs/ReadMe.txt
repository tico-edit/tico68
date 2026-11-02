tico68 @VERSION@
========================================================================

A small text editor for the classic Macintosh, in the style of tico
and nano.  Each document has its own window, tico's keys are there if
you want them, and nothing is on screen that a 512 x 342 Macintosh Plus
can't spare.

It is one 68000 program, meant for a Macintosh Plus with 4 MB under
System 6 or System 7.0.1, and for later Macs running 68k code.

This is a first version.  It is tested under Mini vMac and has had a
first, good run on a real Macintosh Plus.


CONTENTS
========

  1. Getting started
  2. Files
  3. Selecting and the clipboard
  4. Keys
  5. Preferences
  6. Limits
  7. About


1. GETTING STARTED
==================

Double-click tico68, or a document in the Finder.  Each document has
its own window, title and scroll bars.  There are no tabs or panes.

The window has three parts of its own:

  - The title bar at the top shows the version, the document's name
    and "Modified".
  - The shortcut bar near the bottom lists some useful keys.
  - The status strip, the bottom row beside the horizontal scroll bar,
    shows messages, or else the document's format and "Modified".

Lines don't wrap.  The window scrolls sideways as the cursor moves
past its edge.

The menus are File, Edit, Go, Font and Window.  They work the same in
every keyboard profile; only the keys shown beside the items change.


2. FILES
========

Open reads any file.  If it isn't of type TEXT you're asked first, and
only its data fork is edited.

Save keeps a file's resource fork, type, creator and Finder
information.  New files are type TEXT, creator Tc68.

If a save fails, your text stays in its window.

Line endings
------------

A file keeps the line endings it came with.  The format of the front
document is checked in the File menu.  Choosing another one takes
effect for that document the next time it is saved.

+-----------+-------------+--------------------------+
| Format    | Line ending | Usual on                 |
+-----------+-------------+--------------------------+
| Macintosh | CR          | Macintosh; new documents |
| DOS       | CRLF        | DOS and Windows          |
| Unix      | LF          | Unix and Linux           |
+-----------+-------------+--------------------------+


3. SELECTING AND THE CLIPBOARD
==============================

Drag with the mouse, double-click for a word, or hold Shift with an
arrow key.  Typing replaces the selection.

Cut, Copy and Paste use the Macintosh clipboard.  Cut Line, Copy Line
and Paste Line use tico's own cut buffer, which all documents share.


4. KEYS
=======

Edit > Preferences chooses one of three keyboard profiles.  Macintosh
is the default.

+-------------+--------------------------+
| Profile     | What it is               |
+-------------+--------------------------+
| Macintosh   | The usual Macintosh keys |
| Tico legacy | tico's nano-style keys   |
| Tico modern | tico's modern keys       |
+-------------+--------------------------+

In these tables, Cmd is the Command key.  tico's Ctrl is Cmd here.
Meta is Cmd-Option, or in the tico profiles Esc followed by the key.
The Plus keypad's Clear key is Esc.  Option alone types MacRoman
characters.

Shift with an arrow key extends the selection, in every profile.  A
key with no command shows "Unbound key" in the status strip.

Every profile
-------------

+--------------------+------------------------------------+
| Key                | Command                            |
+--------------------+------------------------------------+
| Arrows             | Move                               |
| Home, End          | As named, if the keyboard has them |
| Page Up, Page Down | As named, if the keyboard has them |
| Return, Enter      | New line                           |
| Tab                | Tab                                |
| Delete             | Backspace                          |
| Forward Delete     | Delete                             |
+--------------------+------------------------------------+

Macintosh
---------

+---------------------------+----------------------------+
| Key                       | Command                    |
+---------------------------+----------------------------+
| Cmd-N, Cmd-O              | New, Open                  |
| Cmd-W                     | Close                      |
| Cmd-S, Shift-Cmd-S        | Save, Save As              |
| Cmd-Q                     | Quit                       |
| Cmd-X, Cmd-C, Cmd-V       | Cut, Copy, Paste           |
| F2, F3, F4                | Cut, Copy, Paste           |
| Cmd-A                     | Select All                 |
| Cmd-K, Shift-Cmd-K        | Cut Line, Copy Line        |
| Cmd-U                     | Paste Line                 |
| Cmd-L                     | Location                   |
| Cmd-`                     | Next window                |
| Cmd-Left, Cmd-Right       | Start of line, end of line |
| Cmd-Up, Cmd-Down          | First line, last line      |
| Option-Left, Option-Right | Previous word, next word   |
| Option-Up, Option-Down    | Page Up, Page Down         |
+---------------------------+----------------------------+

Tico legacy
-----------

+---------------------------+--------------------------------------+
| Key                       | Command                              |
+---------------------------+--------------------------------------+
| Cmd-X, F2                 | Exit: close, and quit after the last |
| Cmd-O, F3                 | Write Out (Save As)                  |
| Cmd-S                     | Save                                 |
| Cmd-R                     | Read File (Open, in a new window)    |
| Cmd-K, F9                 | Cut Line                             |
| Cmd-U, F10                | Paste Line                           |
| Meta-6                    | Copy Line                            |
| Meta-A, Cmd-6             | Set Mark                             |
| Cmd-C, F11                | Location                             |
| Cmd-P, Cmd-N              | Line up, line down                   |
| Cmd-A, Cmd-E              | Start of line, end of line           |
| Cmd-Y, Cmd-V              | Page Up, Page Down                   |
| Meta-\                    | First line                           |
| Meta-/                    | Last line                            |
| Cmd-Left, Meta-Space      | Previous word                        |
| Cmd-Right, Cmd-Space      | Next word                            |
| Meta-Left, Meta-comma     | Previous window                      |
| Meta-Right, Meta-period   | Next window                          |
| Cmd-D, Cmd-H              | Delete, Backspace                    |
| Cmd-I, Cmd-M              | Tab, New line                        |
| Option-Left, Option-Right | Start of line, end of line           |
| Option-Up, Option-Down    | Page Up, Page Down                   |
+---------------------------+--------------------------------------+

Cmd-Y and Cmd-V page up and down here: Cmd-V does not paste.  New,
Close, Quit, Cut, Copy, Paste and Select All have no key in this
profile; use the menus.

Tico modern
-----------

The same as Tico legacy, with these changes:

+-------+---------------------+
| Key   | Command             |
+-------+---------------------+
| Cmd-Q | Exit                |
| Cmd-W | Write Out (Save As) |
| Cmd-O | Read File (Open)    |
| Cmd-P | Location            |
| Cmd-A | Set Mark            |
| Cmd-X | Cut Line            |
| Cmd-C | Copy Line           |
| Cmd-V | Paste Line          |
+-------+---------------------+

Cmd-H, Cmd-G, Cmd-D, Cmd-R, Cmd-T, Cmd-Z, Cmd-Y and Cmd-E do nothing
in this profile.


5. PREFERENCES
==============

Edit > Preferences also sets tico's title bar and shortcut bar,
separately, to Always show or Always hide.  Both show by default.
Hiding them gives their rows to the text.

The Font menu lists the fixed-width fonts and sizes.  The default is
Monaco 9.

Settings are kept in a small text file, tico68 Prefs, in the
Preferences folder, or the System Folder under System 6.  A missing
or damaged file just gives the defaults.


6. LIMITS
=========

Text is kept in memory.  A file of short lines takes about twice its
size.

+-------------------------------------+---------------------+
| Machine                             | Largest file, about |
+-------------------------------------+---------------------+
| 4 MB Plus, System 6, no MultiFinder | 2 MB                |
| System 7 or MultiFinder             | 1 MB                |
+-------------------------------------+---------------------+

Under System 7 or MultiFinder, Get Info can raise the program's memory
size.  A file too big to open gives an alert, and tico68 carries on.

Saving needs room on the disk for a second copy of the file for a
moment.

Big files are slow to open: about 15 KB a second is the estimate for a
Plus.

There is no search, undo or syntax highlighting yet.


7. ABOUT
========

tico68 is tico, by way of tico16, as a Macintosh application.

  Web site:  https://ticoedit.org/
  Source:    https://github.com/tico-edit/tico68

The icon is Tico the Toucan, by Pretianahart.

tico68 is released under the MIT License.
