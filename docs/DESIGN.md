# tico68 — Design Document

**Status:** Draft  
**Target:** Classic Macintosh, Motorola 68000 or later; PowerPC through 68k emulation  
**Primary test machine:** Macintosh Plus (“wombat”), 4 MB RAM  
**Implementation:** C, Retro68  
**Proposed repository:** `tico-edit/tico68`  
**License:** MIT (subject to confirmation for any reused dependencies)

## 1. Overview

**tico68** is a small, native classic Macintosh text editor inspired by [tico](https://github.com/tico-edit/tico) and built primarily from the portable C foundations of [tico16](https://github.com/tico-edit/tico16). It preserves tico's editing behavior and familiar keyboard bindings while behaving like a real Macintosh application: windows, menus, multiple documents, mouse interaction, and Macintosh file handling.

The principal design constraint is usability on an 8 MHz Macintosh Plus with a 512 × 342 monochrome screen. At the same time, the same binary should remain useful on later 68k Macintosh systems, including Color Classics and Quadras, and on a PowerPC Performa 6360 (“chimera”) through classic Mac OS 68k emulation.

It is **not** a terminal emulator or an 80 × 25 DOS interface transplanted wholesale to Macintosh. Its optional tico-style title and shortcut bars preserve the editor's identity without consuming scarce display space when they would be counterproductive.

## 2. Principles

1. Reuse `tico16`'s portable text and buffer code where practical; do not port the Rust terminal implementation of tico directly.
2. Preserve the behavior and muscle memory of tico, including its **legacy and modern** binding profiles, while offering normal Macintosh conventions as a separate profile.
3. Preserve each file's detected line-ending format by default, and allow explicit per-document conversion.
4. Be responsive and economical with RAM on a Macintosh Plus.
5. Use standard classic Macintosh Toolbox APIs and avoid direct hardware/framebuffer assumptions.
6. Keep the first version deliberately small, but implement **multiple open documents** from the outset.
7. Build on modern Linux using Retro68 and test under Mini vMac and physical hardware.
8. Document deliberate differences from tico/tico16 rather than silently diverging.

## 3. Supported systems

### Required

- Macintosh Plus, 68000 at 8 MHz, 4 MB RAM.
- System 6 and System 7.0.1.
- 512 × 342 monochrome display.

### Intended compatibility

- Later 68000/68020/68030/68040 Macintosh systems running compatible classic Mac OS versions.
- Larger and color displays, including the Color Classic's 512 × 384 resolution.
- PowerPC Macintosh systems that execute 68k applications via the OS emulator, including the Performa 6360 (“chimera”).

Use an original **68000 instruction-set target** with no FPU requirement. The binary should be a conventional 68k Macintosh application using `CODE`, `SIZE`, and other requisite resource metadata. Do not depend on Color QuickDraw, System 7-only features, MultiFinder, or PowerPC-native code for baseline operation. Broader System 7/8/9 compatibility is a testing goal, **not an unverified guarantee**.

### Display model

Do not assume a fixed screen resolution. Determine usable window bounds through the Toolbox and relayout on window resize. UI decisions must use the **available document viewport/window dimensions**, not CPU type or machine identity.

## 4. Application model and multiple documents

- Each open document has a separate native Macintosh window.
- `File → New` creates a blank document; `File → Open…` can open another without replacing the active one.
- Each document independently stores its text buffer, filename and file identity, encoding/byte handling state, line-ending format, modified flag, cursor/selection, scrolling position, and display layout state.
- A **Window** menu lists open documents and permits switching between them.
- `Save`, `Save As…`, and `Close` act on the active document.
- Closing a modified document prompts to save, discard, or cancel; quitting checks all modified documents without silently discarding edits.
- Multiple windows must work in a single application process under ordinary System 6 Finder; do not require MultiFinder.
- Text can be copied between open documents. Macintosh Scrap Manager clipboard integration is desired in the initial release.
- Clicking a document activates it; basic mouse selection and drag selection should be supported in the first usable release.

The design should distinguish **document state**, **editor commands**, **window/UI state**, and **platform services** rather than making the existing DOS event loop the center of the application.

## 5. Text editing and behavior

Port the essential functionality already implemented in `tico16`:

- Insert and delete characters; Enter, Tab, Backspace, Delete.
- Cursor movement by characters, lines, line start/end, and file start/end.
- Page Up and Page Down.
- Horizontal and vertical scrolling without automatic soft wrapping initially.
- Cut/copy/paste line operations and modified-state tracking.
- Save, Save As, Close, and New/Open document.
- Cursor-location reporting.
- Familiar tico-style status and error messages where practical.

Use the regular tico implementations as behavioral references, with `tico16` as the C implementation base. Search, undo/redo, region operations, and other advanced tico features can follow in later releases unless needed to make basic mouse selection and clipboard operations coherent.

## 6. Keyboard profiles

Keyboard shortcuts are configurable and independent of menu availability, window behavior, font choices, and visibility of the tico bars.

Three profiles are required:

1. **tico legacy** — older tico bindings and editing muscle memory.
2. **tico modern** — tico's newer binding set.
3. **Macintosh** — normal classic Mac application conventions.

The exact legacy/modern maps must be derived from the **actual tico keybinding definitions** and recorded in a reference table before implementation; don't guess where they differ. The source profiles use Control/Meta conventions, whereas the Mac Plus keyboard may lack a Control key. Map relevant tico Control combinations to **Command (⌘)** and define a practical treatment of Meta/Escape sequences. Key conflicts are resolved by the chosen profile; Macintosh conventions **must not silently override tico legacy mode**.

### Confirmed legacy requirements

| tico legacy on Macintosh | Action |
| --- | --- |
| **⌘Y** | **Page Up** |
| **⌘V** | **Page Down** |
| **⌘U** | Paste tico cut buffer |

In particular, **⌘V must not paste in legacy mode**. This intentional conflict with the conventional Macintosh paste shortcut is part of the legacy profile, not a bug.

### Macintosh profile

Conventional actions should include ⌘N New, ⌘O Open, ⌘S Save, ⌘W Close, ⌘Q Quit, and ⌘X/⌘C/⌘V cut/copy/paste. Save As should be provided through the File menu and an appropriate shortcut where supported. Editing commands that lack a standard Macintosh shortcut remain available through menus and/or additional bindings.

All profiles map physical key events to the **same editor-command layer**. Menus must remain functional regardless of active profile. Menu shortcut labels and the optional tico shortcut bar must reflect the **active** profile. The user can change profiles in Preferences, and the choice persists between launches.

## 7. Tico-style title and shortcut bars

The program name/title bar at the top of the editing area and shortcut help at the bottom are recognizable parts of tico's identity, but expensive in vertical pixels on wombat.

**Both bars are independently configurable**:

- `Always show` (default)
- `Always hide`

The choice applies irrespective of window size. Preferences are global, persist across launches, and remain independent of the active keyboard profile. A native window title continues to identify the file even when the internal tico title bar is hidden. The bars should never be necessary to discover core commands because those remain accessible through menus.

The shortcut bar must display correct shortcuts for the selected profile. If a bar would not fit horizontally, show fewer shortcuts rather than clipping or misleading the user.

## 8. Fonts and text layout

- **Fixed-width font by default and required for the initial implementation.**
- Proposed initial setting: **Monaco, 9 point, regular**, subject to legibility tests on the physical Macintosh Plus.
- Permit selection among available **monospaced** fonts and point sizes through Preferences or a Font menu.
- Persist the choice across launches (disk-stored preferences; no reliance on PRAM battery).
- Keep text content/column calculation, cursor placement, tab stops, and horizontal scrolling correct for the selected fixed-width font.
- Use QuickDraw text rendering. Prefer partial/incremental invalidation/redraw over full-window repaint for each keystroke.
- No requirement for syntax highlighting or proportional font support in v1.
- Font settings are independent of the optional tico bars, which may have their own compact appearance.

## 9. Text file formats and byte fidelity

### Detection and default preservation

Reuse `tico16`'s existing logic for recognizing **the first line break** and preserving the format when writing:

| Detected format | Line ending | Default output |
| --- | --- | --- |
| Macintosh | CR (`\r`) | CR |
| DOS/Windows | CRLF (`\r\n`) | CRLF |
| Unix | LF (`\n`) | LF |
| New document | — | Macintosh CR |

Files are initially treated as **8-bit byte-oriented text**, without automatic Unicode conversion. Preserve normal file contents, tabs, and high-bit bytes through the buffer/serialization layer; define any MacRoman display policy separately from byte storage. Retain `tico16`'s documented treatment of mixed endings and final newlines unless deliberately changed and recorded in `DIFFERENCES.md`.

Opening and saving an unchanged, consistently formatted file with an existing final newline should preserve the bytes exactly, aside from intentional format conversion. Existing special handling of trailing DOS Ctrl+Z should be reviewed and maintained when appropriate.

### Per-document text-format menu

Provide **File → Text Format** with three mutually exclusive, checkmarked options:

- **Macintosh (CR)**
- **DOS / Windows (CRLF)**
- **Unix (LF)**

On open, select the detected format. Each document keeps its own selected output format. Choosing another format changes the intended **serialization**, not the in-memory text, cursor, or selection; mark the document modified if the on-disk output would change. Conversion occurs on the next save, not immediately on menu selection. When a document is reopened, detect its actual format again.

A menu checkmark is always available as an indicator even when the internal tico bars are hidden. A lightweight optional status display may also show the format, without requiring a permanent status strip on a 512 × 342 screen.

## 10. Classic Macintosh file handling

- Use classic Mac File Manager / Standard File interfaces for opening and saving files; the initial implementation should be compatible with System 6.
- Preserve existing files' Finder **type/creator** and other metadata when saving, whenever possible.
- Do not accidentally erase existing **resource forks** when editing a file's data fork.
- New plain-text files should receive a suitable classic Macintosh text file type and documented creator policy.
- Respect locked/read-only volumes and files; handle full disks and I/O failures without discarding the editor buffer.
- Do not directly reuse the DOS `NAME.$$$` delete/rename save algorithm: design a Mac-appropriate safe write/replace path, mindful of resource forks, permissions/locks, metadata, and recovery from partial failure.
- Clearly distinguish the text **data fork** from application resources.
- Decide how to handle opening files with non-text type codes, especially if the user explicitly chooses them.
- Save As may change filename and volume; its default text format remains the document's currently selected format.

## 11. Architecture and source reuse

Expected adaptation of `tico16`:

| Source | Plan |
| --- | --- |
| `src/buffer.c` | Reuse portable per-line buffer logic, adapting allocator and ownership as required |
| `src/text.c` | Reuse parsing/serialization and line-ending detection; default new Mac files to CR |
| `src/editor.c` | Extract reusable commands and document operations; replace its DOS event-loop/display coupling |
| `src/dos.c` | **Do not port directly**; implement Mac event, window, drawing, keyboard, menu, clipboard, and file services |
| `src/main.c` | New Macintosh application startup and event loop |
| `src/tico.h` | Refactor platform-facing declarations and document/UI types |

The reusable C core should remain host-buildable for unit tests. Extract genuinely shared components cleanly rather than creating a permanent wholesale fork, but avoid making a new common library an independent prerequisite if it slows initial delivery. Coordinate changes with `tico16` through tested, clearly defined interfaces.

Suggested logical modules:

- **Core:** text buffer, line-ending parser/writer, editing commands.
- **Document:** filename, format, modified state, cursor and selection, open/save lifecycle.
- **Mac platform:** Toolbox events, windows, QuickDraw, menus/dialogs, Scrap Manager, files, preferences.
- **Presentation:** text viewport and optional title/shortcut bars.
- **Bindings:** profile-specific key-to-command mapping.

### Implementation considerations

Classic Macintosh application architecture is event-driven. Process keyboard, mouse, update, activate, resize, and menu events through the Toolbox event loop. Do not block waiting for editor keystrokes. Use classic memory allocation practices compatible with 68000/System 6; measure realistic memory usage with several documents open. Do not assume every available byte of physical RAM is application heap.

## 12. Preferences

Preferences must persist on disk without relying on PRAM. At minimum:

| Preference | Values | Initial default |
| --- | --- | --- |
| Keybinding profile | tico legacy / tico modern / Macintosh | Macintosh (proposed; user-selectable) |
| Internal tico title bar | Show / Hide | Show |
| Internal tico shortcut bar | Show / Hide | Show |
| Document font | Available fixed-width faces | Monaco |
| Document font size | Supported point sizes | 9 pt |

The selected output format belongs to each **document**, not to the global preferences. A newly created document defaults to Macintosh CR. Preferences should fail gracefully if missing or unreadable.

## 13. Testing

### Host-based tests

- Existing `tico16` buffer and text-unit tests remain applicable.
- Add round-trip fixtures for CR, CRLF, LF, empty files, final newlines, mixed endings, tabs, control bytes, high-bit bytes, NUL bytes, and trailing DOS Ctrl+Z behavior.
- Test explicit per-document format changes without modifying other open documents.
- Test command dispatch for all three keyboard profiles, particularly **legacy ⌘Y / ⌘V**.
- Test document closing, dirty-state prompts, and save failure behavior through separable core/platform abstractions where practical.

### Emulator tests

Under Mini vMac configured as Macintosh Plus/4 MB:

- Start and operate on System 6 and System 7.0.1.
- Open, edit, save, and switch between several documents.
- Test all keyboard modes; menus work even in legacy mode.
- Resize windows and inspect the bars.
- Confirm fixed-width text, scrolling, cursor placement, and selection.
- Verify Finder metadata/resource fork behavior with representative files.

### Real-hardware tests

On **wombat**:

- Check startup time, input responsiveness, redraw performance, and memory use.
- Confirm comfortable use at 512 × 342 with both internal bars shown and with both hidden.
- Edit and save files on its BlueSCSI data volume without corruption.
- Test System 6 and System 7.0.1.

On **chimera** (or another later Macintosh), verify larger-window behavior, the bars, clipboard, and 68k emulator compatibility. Color Classic testing is desirable when hardware becomes available; do not block the first release on it.

## 14. Build, packaging, and distribution

- Build on Linux with **Retro68**, explicitly targeting the 68000.
- Use a reproducible CMake-based build or another documented Retro68-compatible build setup.
- Generate a classic 68k `APPL` with appropriate resource metadata.
- Provide a transfer-friendly **800K HFS disk image** compatible with wombat's Floppy Emu; optionally provide MacBinary or StuffIt-compatible distribution artifacts as practical.
- Keep resource forks and type/creator metadata intact across packaging and transfer.
- Maintain unit tests and build checks in CI; real-hardware acceptance remains separate.
- Prefer the same repository/versioned-release discipline as `tico16`, with specific workflow mechanics settled during implementation.

## 15. Initial scope and deferred features

### Required in first usable release

- Native multi-window Mac application.
- Edit, navigate, cut/copy/paste, open/new, save/save as, close/quit with unsaved prompts.
- Three selectable keyboard profiles, including confirmed legacy page bindings.
- Detect and preserve CR/CRLF/LF; per-document **File → Text Format** override.
- Fixed-width font (default Monaco 9 point).
- Independently configurable internal title and shortcut bars.
- Correct Macintosh file metadata/resource fork handling and recoverable I/O errors.
- System 6 and 7.0.1 operation on a physical Macintosh Plus.

### Explicitly deferred unless technically necessary

- Syntax highlighting and tree-sitter.
- Plug-ins, scripting, themes, and sophisticated configuration.
- Proportional fonts.
- Multiple panes/tabs; each document uses a native window.
- Advanced search/replace, undo/redo, macros, programmable bindings, and most newer tico features.
- PowerPC-native build.

## 16. Open design questions

1. What are the precise definitions of tico's **legacy vs modern** bindings, and how should original Meta sequences map to the Macintosh keyboard?
2. Which Shortcut and menu accelerators are conflict-free under each profile, especially when the legacy profile uses ⌘V for Page Down?
3. ~~What viewport-height/visible-row thresholds work best for automatic bar visibility without oscillation on resize?~~ Dropped: the bars no longer have an automatic setting.
4. Should the full text format be shown in the native window title, a minimal status indicator, or only as a checked menu item?
5. What is the best classic Mac safe-save strategy for maintaining Finder metadata and resource forks while minimizing risk of data loss?
6. How should MacRoman glyph display interact with raw 8-bit byte preservation for files authored on DOS/Unix systems?
7. What is the least complex, reliable shared-source strategy with `tico16` as both projects evolve?
8. What are the smallest practical memory footprint and supported maximum document size on a 4 MB Plus?

## 17. Acceptance criteria

The initial release is accepted when a single 68000-compatible binary:

1. Launches and edits responsively on **wombat** under **System 6** and **System 7.0.1**.
2. Opens multiple independent files/windows and safely saves or closes each.
3. Preserves detected Macintosh, DOS, and Unix line endings by default.
4. Allows changing the current document's format through **File → Text Format**, applying it on save without changing other documents.
5. Supports tico legacy, tico modern, and Macintosh shortcut profiles; **⌘Y is Page Up and ⌘V is Page Down in legacy mode**.
6. Uses a fixed-width font by default and offers font selection among monospaced options.
7. Allows separate Show/Hide preferences for tico's title and shortcut bars, both shown by default; hiding them yields a compact usable display on a 512 × 342 screen.
8. Preserves existing file metadata/resource forks as designed, and does not lose the in-memory buffer after recoverable file errors.
9. Can be built with Retro68 on Linux and delivered as a usable classic Macintosh disk image.
10. Has documented usage, configuration, intentional differences from tico/tico16, building, and test coverage.

---

**Summary:** `tico16`'s portable editor foundation, tico's recognizable behavior, and a proper classic Macintosh multi-document interface—without wasting wombat's precious pixels.
