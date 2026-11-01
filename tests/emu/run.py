#!/usr/bin/env python3
"""Acceptance run: drive tico68 under Mini vMac, as a 4 MB Macintosh Plus,
then check what it left on disk with hfsutils.

Usage: run.py [--system 6|7] [--keep DIR]

Needs the boot disk from macplus-pram-r's tools/make-emulator-disk.sh
(BOOT_DSK, or ~/dev/macplus-pram-r/images/wombat-hd.dsk), which is copied,
never written.  Screenshots go in the work directory.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import macemu  # noqa: E402
import mkfixtures  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(HERE))
RETRO68 = os.environ.get("RETRO68",
                         os.path.expanduser("~/opt/retro68/git/toolchain"))
BOOT = os.environ.get("BOOT_DSK", os.path.expanduser(
    "~/dev/macplus-pram-r/images/wombat-hd.dsk"))

# Where things are on the 512 x 342 screen.
FILE_X, EDIT_X, GO_X = 50, 92, 130
FILE_DOS, FILE_UNIX, FILE_MAC = 172, 188, 156
FILE_QUIT, FILE_SAVE = 220, 92
EDIT_PREFS = 252
SAVE_BUTTON = (371, 186)
REPLACE_BUTTON = {"6": (169, 203), "7": (330, 165)}     # Yes, or Replace
DONT_SAVE_BUTTON = (153, 186)
LEGACY_RADIO = (142, 87)
MODERN_RADIO = (142, 105)
MAC_RADIO = (142, 123)
TITLE_AUTO, TITLE_SHOW = (142, 167), (142, 185)
KEYS_AUTO, KEYS_SHOW = (286, 167), (286, 185)

failures = []
SYSTEM = "7"


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name + (" " + detail if detail else ""))
    if not ok:
        failures.append(name)


class Disk:
    """Read a disk image with hfsutils."""

    def __init__(self, path, scratch):
        self.path = path
        self.scratch = scratch
        self.env = dict(os.environ, HOME=scratch)
        self.env["PATH"] = os.path.join(RETRO68, "bin") + ":" + self.env["PATH"]

    def run(self, *args):
        return subprocess.run(args, env=self.env, check=True,
                              capture_output=True)

    def __enter__(self):
        self.run("hmount", self.path)
        return self

    def __exit__(self, *exc):
        self.run("humount")

    def macbinary(self, name):
        """type, creator, data fork, resource fork."""
        out = os.path.join(self.scratch, "out.bin")
        self.run("hcopy", "-m", ":" + name, out)
        with open(out, "rb") as f:
            b = f.read()
        dlen, rlen = struct.unpack(">II", b[83:91])
        rstart = 128 + dlen + (-dlen % 128)
        return b[65:69], b[69:73], b[128:128 + dlen], b[rstart:rstart + rlen]

    def names(self):
        return self.run("hls", "-1").stdout.decode("mac_roman").split("\n")


def open_file(m, name):
    m.cmd("o")
    time.sleep(1.5)
    m.type(name)
    time.sleep(0.5)
    m.key("Return")
    time.sleep(1.5)


def launch(m):
    """Open the data disk, and start tico68 from it."""
    m.click(473, 115, repeat=2)
    time.sleep(4)
    start_app(m)


def start_app(m):
    if SYSTEM == "6":
        # no type-to-select in this Finder: the icon is where it lands
        m.click(110, 168, repeat=2)
    else:
        m.type("tico")
        time.sleep(0.5)
        m.cmd("o")
    time.sleep(6)


def session(m):
    launch(m)
    m.save("01-launched")

    # Edit an existing Macintosh file, and save it.
    open_file(m, "mac.txt")
    m.save("02-mac-open")
    m.type("X")
    m.cmd("s")
    time.sleep(1.5)
    m.save("03-mac-saved")
    m.cmd("w")
    time.sleep(0.5)

    # Convert a DOS file to Unix through the File menu, with another
    # document open, which must be left alone.
    open_file(m, "unix.txt")
    open_file(m, "dos.txt")
    m.menu(FILE_X, FILE_UNIX)
    time.sleep(0.5)
    m.save("04-dos-to-unix")
    m.cmd("s")
    time.sleep(1.5)
    m.cmd("w")
    time.sleep(0.5)
    m.cmd("s")          # the unix file: unmodified, saved as it was
    time.sleep(1.5)
    m.cmd("w")
    time.sleep(0.5)

    # A file with a resource fork and someone else's creator.
    open_file(m, "forked")
    m.type("Y")
    m.cmd("s")
    time.sleep(1.5)
    m.cmd("w")
    time.sleep(0.5)

    # A new document, saved under a name.
    m.cmd("n")
    time.sleep(1)
    m.type("brand new")
    m.key("Return")
    m.cmd("s")
    time.sleep(1.5)
    m.save("05-save-as")
    m.type("new.txt")
    m.key("Return")
    time.sleep(2)
    m.save("06-saved-new")
    m.cmd("w")
    time.sleep(0.5)

    # Save As over another file, with its usual backup name taken: the
    # backup has to find another, and that file must be left alone.
    for name, text in (("over.txt.bak", "keep me"), ("over.txt", "victim")):
        m.cmd("n")
        time.sleep(1)
        m.type(text)
        m.cmd("s")
        time.sleep(1.5)
        m.type(name)
        m.key("Return")
        time.sleep(2)
        m.cmd("w")
        time.sleep(0.5)
    m.cmd("n")
    time.sleep(1)
    m.type("replacement")
    m.cmd("s")
    time.sleep(1.5)
    m.type("over.txt")
    m.key("Return")
    time.sleep(1.5)
    m.save("06a-replace-prompt")
    m.click(*REPLACE_BUTTON[SYSTEM])
    time.sleep(2)
    m.save("06b-replaced")
    m.cmd("w")
    time.sleep(0.5)

    # A locked file: saving fails, and the text survives in the window.
    open_file(m, "locked")
    m.type("Z")
    m.cmd("s")
    time.sleep(2)
    m.save("07-locked-alert")
    m.key("Return")
    time.sleep(1)
    m.save("08-locked-buffer-kept")
    m.cmd("w")
    time.sleep(1)
    m.save("09-close-prompt")
    m.click(*DONT_SAVE_BUTTON)
    time.sleep(1)

    # A file that isn't TEXT asks first.
    m.cmd("o")
    time.sleep(1.5)
    m.type("binary")
    time.sleep(0.5)
    m.key("Return")
    time.sleep(1.5)
    m.save("10-binary-alert")
    m.key("Return")
    time.sleep(1.5)
    m.save("11-binary-open")
    m.cmd("w")
    time.sleep(0.5)

    mouse_and_clipboard(m)
    appearance(m)

    # Switch to tico legacy: Command-V must page, not paste.
    m.menu(EDIT_X, EDIT_PREFS)
    time.sleep(1.5)
    m.save("12-preferences")
    m.click(*LEGACY_RADIO)
    m.save("13-legacy-chosen")
    m.key("Return")
    time.sleep(1)
    m.move(EDIT_X, 8)
    m.xdo("mousedown", 1)
    time.sleep(1)
    m.save("14-legacy-edit-menu")
    m.move(EDIT_X, 330)
    m.xdo("mouseup", 1)
    time.sleep(0.5)
    m.cmd("r")          # ^R, Read File
    time.sleep(1.5)
    m.type("long.txt")
    time.sleep(0.5)
    m.key("Return")
    time.sleep(4)
    m.save("15-long-open")
    m.cmd("v")
    time.sleep(1)
    m.save("16-legacy-cmd-v-pages-down")
    m.cmd("v")
    time.sleep(1)
    m.cmd("y")
    time.sleep(1)
    m.save("17-legacy-cmd-y-pages-up")
    m.cmd("c")          # ^C, Location
    time.sleep(1)
    m.save("18-legacy-location")
    m.cmd("k")          # ^K
    m.cmd("u")          # ^U
    m.cmd("u")
    time.sleep(1)
    m.cmd("s")          # ^S
    time.sleep(3)
    m.save("19-legacy-saved")

    # Meta: Command-Option-/ to the last line, and back with Esc then \
    m.cmd("slash", "Alt_L")
    time.sleep(1)
    m.save("20-meta-last-line")
    m.key("Escape")
    m.key("backslash")
    time.sleep(1)
    m.save("21-esc-meta-first-line")

    # Back to Macintosh keys, so the next launch shows they persisted...
    # and quit with a modified document, saving it.
    m.menu(EDIT_X, EDIT_PREFS)
    time.sleep(1.5)
    m.click(*MODERN_RADIO)
    m.key("Return")
    time.sleep(1)
    m.type("Q")
    m.menu(FILE_X, FILE_QUIT)
    time.sleep(2)
    m.save("22-quit-prompt")
    m.click(*SAVE_BUTTON)
    time.sleep(4)
    m.save("23-quit")

    # Launch again: modern must have stuck.
    start_app(m)
    m.move(EDIT_X, 8)
    m.xdo("mousedown", 1)
    time.sleep(1)
    m.save("24-modern-persisted")
    m.move(EDIT_X, 330)
    m.xdo("mouseup", 1)


def cell(col, row, origin=(4, 40)):
    """Screen position of a text cell in the front window."""
    x, y = origin
    return x + 4 + col * 6 + 1, y + row * 11 + 5


def find_window(m):
    """Where the front window's content starts.  Windows step down the
    screen, so go by the title bar: only the active one has stripes."""
    img = m.shot().convert("L")

    def dark(y):
        return sum(1 for x in range(512) if img.getpixel((x, y)) < 128)

    for y in range(21, 240):
        if dark(y) > 150 and dark(y + 1) < 60 and dark(y + 2) > 150:
            # windows step by 18 pixels both ways, from (4, 40)
            steps = round((y + 15 - 40) / 18)
            return 4 + steps * 18, 40 + steps * 18
    raise RuntimeError("no active window")


def mouse_and_clipboard(m):
    m.cmd("n")
    time.sleep(1)
    WIN = find_window(m)
    m.type("alpha beta")
    m.key("Return")
    m.type("gamma delta")
    time.sleep(0.5)
    # drag across "alpha ", cut it, paste it at the end of the document
    x0, y0 = cell(0, 0, WIN)
    x1, y1 = cell(6, 0, WIN)
    m.drag(x0, y0, x1, y1, hold=0.5)
    time.sleep(0.5)
    m.save("30-drag-selection")
    m.cmd("x")
    time.sleep(0.8)
    m.click(*cell(30, 1, WIN))
    m.cmd("v")
    time.sleep(0.8)
    m.save("31-cut-and-paste")
    # double-click selects a word; Copy; paste it into another document
    m.click(*cell(2, 0, WIN), repeat=2)
    time.sleep(0.5)
    m.save("32-word-selected")
    m.cmd("c")
    time.sleep(0.5)
    m.cmd("n")
    time.sleep(1)
    m.cmd("v")
    time.sleep(0.8)
    m.move(205, 8)
    m.xdo("mousedown", 1)
    time.sleep(1)
    m.save("33-window-menu")
    m.move(205, 330)
    m.xdo("mouseup", 1)
    time.sleep(0.5)
    m.cmd("s")
    time.sleep(1.5)
    m.type("clip.txt")
    m.key("Return")
    time.sleep(2)
    m.cmd("w")
    time.sleep(1)
    m.cmd("s")
    time.sleep(1.5)
    m.type("moved.txt")
    m.key("Return")
    time.sleep(2)
    m.cmd("w")
    time.sleep(1)


def appearance(m):
    # both bars on, on a screen that wouldn't choose them
    m.cmd("n")
    time.sleep(1)
    m.menu(EDIT_X, EDIT_PREFS)
    time.sleep(1.5)
    m.click(*TITLE_SHOW)
    m.click(*KEYS_SHOW)
    m.key("Return")
    time.sleep(2)
    m.type("bars on")
    time.sleep(1)
    m.save("34-both-bars")
    # the Font menu
    m.move(162, 8)
    m.xdo("mousedown", 1)
    time.sleep(1)
    m.save("35-font-menu")
    m.move(162, 330)
    m.xdo("mouseup", 1)
    time.sleep(0.5)
    # smaller window: the bar must drop shortcuts rather than clip them
    m.drag(500, 331, 400, 240, hold=0.5)
    time.sleep(2)
    m.save("36-resized-narrow")
    m.menu(EDIT_X, EDIT_PREFS)
    time.sleep(1.5)
    m.click(*TITLE_AUTO)
    m.click(*KEYS_AUTO)
    m.key("Return")
    time.sleep(2)
    m.save("37-bars-auto")
    m.cmd("w")
    time.sleep(1)
    m.click(*DONT_SAVE_BUTTON)
    time.sleep(1)


def verify(work):
    scratch = os.path.join(work, "hfs")
    with Disk(os.path.join(work, "data.dsk"), scratch) as d:
        t, c, data, rsrc = d.macbinary("mac.txt")
        check("edited CR file", data == b"Xmac one\rmac two\rmac three\r",
              repr(data))
        check("type and creator kept", (t, c) == (b"TEXT", b"ttxt"))

        t, c, data, rsrc = d.macbinary("dos.txt")
        check("DOS file converted to LF", data == b"dos one\ndos two\n",
              repr(data))

        t, c, data, rsrc = d.macbinary("unix.txt")
        check("other document untouched", data == b"unix one\nunix two\n",
              repr(data))

        t, c, data, rsrc = d.macbinary("forked")
        check("forked data", data == b"Yhas a fork\r", repr(data))
        check("resource fork kept",
              rsrc == mkfixtures.FILES["forked"][3], "%d bytes" % len(rsrc))
        check("foreign creator kept", (t, c) == (b"TEXT", b"MSWD"))

        t, c, data, rsrc = d.macbinary("new.txt")
        # nano's Enter on the line before the magic line: "a\\n\\n"
        check("new file is CR", data == b"brand new\r\r", repr(data))
        check("new file type", (t, c) == (b"TEXT", b"Tc68"), repr((t, c)))

        t, c, data, rsrc = d.macbinary("locked")
        check("locked file unchanged", data == b"locked file\r", repr(data))

        t, c, data, rsrc = d.macbinary("long.txt")
        want = mkfixtures.FILES["long.txt"][2]
        lines = data.split(b"\r")
        check("long file: one more line, all CR",
              len(lines) == 2002 and b"\n" not in data,
              "%d lines" % (len(lines) - 1))
        check("long file: Q typed first", data.startswith(b"Q"))

        t, c, data, rsrc = d.macbinary("moved.txt")
        check("cut and paste by mouse", data == b"beta\rgamma deltaalpha \r",
              repr(data))
        t, c, data, rsrc = d.macbinary("clip.txt")
        check("paste into another document", data == b"beta\r", repr(data))

        t, c, data, rsrc = d.macbinary("over.txt")
        check("Save As replaced the file", data == b"replacement\r",
              repr(data))
        t, c, data, rsrc = d.macbinary("over.txt.bak")
        check("taken backup name left alone", data == b"keep me\r",
              repr(data))

        leftovers = [n for n in d.names()
                     if n.endswith((".tmp", ".bak2"))]
        check("no temporary files left", not leftovers, repr(leftovers))
    return want


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--system", choices=["6", "7"], default="7")
    ap.add_argument("--keep")
    args = ap.parse_args()
    global SYSTEM
    SYSTEM = args.system

    work = args.keep or tempfile.mkdtemp(prefix="tico68-emu")
    os.makedirs(work, exist_ok=True)
    scratch = os.path.join(work, "hfs")
    os.makedirs(scratch, exist_ok=True)
    boot = os.path.join(work, "boot.dsk")
    subprocess.run(["cp", "--reflink=auto", BOOT, boot], check=True)
    env = dict(os.environ, HOME=scratch)
    env["PATH"] = os.path.join(RETRO68, "bin") + ":" + env["PATH"]
    os.environ["PATH"] = env["PATH"]

    subprocess.run(["hmount", boot], env=env, check=True,
                   stdout=subprocess.DEVNULL)
    folder = ":System Folder" if args.system == "6" else ":System Folder 7.0.1"
    subprocess.run(["hattrib", "-b", folder], env=env, check=True)
    subprocess.run(["humount"], env=env, check=True)

    data = os.path.join(work, "data.dsk")
    subprocess.run([sys.executable, os.path.join(HERE, "mkfixtures.py"),
                    data, scratch], check=True)
    subprocess.run(["hmount", data], env=env, check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run(["hcopy", "-m",
                    os.path.join(ROOT, "build/mac/tico68.bin"), ":tico68"],
                   env=env, check=True)
    subprocess.run(["humount"], env=env, check=True)

    with macemu.Mac(work, [boot, data]) as m:
        time.sleep(16)
        m.save("00-booted")
        try:
            session(m)
        finally:
            m.save("99-end")
            time.sleep(2)
    verify(work)
    print("screenshots in", work)
    if failures:
        print(len(failures), "failed:", ", ".join(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
