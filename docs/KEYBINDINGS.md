# Keyboard profiles

Every profile reaches the same commands. Choose one with
Edit → Preferences…; the ⌘ keys shown in the menus change with it. This
reference comes from the tables in `src/bindings.c`, and the legacy and
modern tables there from tico's `src/keymap.rs`.

## How keys map

| tico writes | Press |
| --- | --- |
| `^X` (Ctrl) | ⌘X, or Control-X on a keyboard with Control |
| `M-X` (Meta) | ⌘-Option-X; in the tico profiles also Esc then X |
| Esc | Esc, or Clear on a Plus keypad |

Option alone types MacRoman characters. Esc pressed twice cancels Meta.
Shift with an arrow or other non-character movement key extends the
selection, in every profile. A key
with no binding shows `Unbound key: KEY` in the status strip.

## Every profile

| Key | Command |
| --- | --- |
| Arrows | Move |
| Home, End, Page Up, Page Down | As named, where the keyboard has them |
| Return, Enter | New line |
| Tab | Tab |
| Delete (⌫) | Backspace |
| Forward Delete (⌦) | Delete |

## Tico legacy

| Key | Command |
| --- | --- |
| ⌘X, F2 | Exit: close the window, then quit after the last one |
| ⌘O, F3 | Write Out (Save As) |
| ⌘S | Save |
| ⌘R | Read File (Open, in a new window) |
| ⌘K, F9 | Cut Line |
| ⌘U, F10 | Paste Line |
| M-6, M-^ | Copy Line |
| M-A, ⌘6, ⌘^ | Set Mark |
| ⌘C, F11 | Location |
| ⌘P, ⌘N | Line up, line down |
| ⌘A, ⌘E | Start of line, end of line |
| **⌘Y, ⌘V** | **Page Up, Page Down** |
| M-\\, ⌘Home | First line |
| M-/, ⌘End | Last line |
| ⌘←, M-Space | Previous word |
| ⌘→, ⌘Space | Next word |
| M-←, M-comma, M-< | Previous window |
| M-→, M-period, M-> | Next window |
| ⌘D, ⌘H | Delete, Backspace |
| ⌘I, ⌘M | Tab, New line |
| Option-←, Option-→ | Start of line, end of line |
| Option-↑, Option-↓ | Page Up, Page Down |

New, Close, Quit, Cut, Copy, Paste and Select All have no key here; use
the menus. So ⌘Q and ⌘W report an unbound key: `^Q` and `^W` are searches
in tico, which tico68 doesn't have.

## Tico modern

The legacy table, with these changes.

| Key | Command |
| --- | --- |
| ⌘Q | Exit |
| ⌘W | Write Out (Save As) |
| ⌘O | Read File (Open) |
| ⌘P | Location |
| ⌘A | Set Mark |
| ⌘X | Cut Line |
| ⌘C | Copy Line |
| ⌘V | Paste Line |

⌘H, ⌘G, ⌘D, ⌘R, ⌘T, ⌘Z, ⌘Y and ⌘E are help, search, undo and the like in
tico. They are unbound here, rather than keeping their legacy meaning.

## Macintosh

| Key | Command |
| --- | --- |
| ⌘N, ⌘O | New, Open |
| ⌘W | Close |
| ⌘S, Shift-⌘S | Save, Save As |
| ⌘Q | Quit |
| ⌘X, F2 | Cut |
| ⌘C, F3 | Copy |
| ⌘V, F4 | Paste |
| ⌘A | Select All |
| ⌘K, Shift-⌘K | Cut Line, Copy Line |
| ⌘U | Paste Line |
| ⌘L | Location |
| ⌘\` | Next window |
| ⌘←, ⌘→ | Start of line, end of line |
| ⌘↑, ⌘↓, ⌘Home, ⌘End | First line, last line |
| Option-←, Option-→ | Previous word, next word |
| Option-↑, Option-↓ | Page Up, Page Down |

## Why not the Toolbox's menu keys

Keys go straight to these tables and never through `MenuKey`, or legacy
⌘V would paste. The ⌘ letters in the menus are labels set for the
profile; a key that a menu can't show as a single letter is appended to
the item's text, as in `M-6` or `Sh-⌘S`.
