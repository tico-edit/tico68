"""Drive Mini vMac through macman, headlessly: Xvfb for a display, xdotool for
input, and Xvfb's framebuffer file for screenshots.

Each run gets its own throwaway machine, cloned by macman from a registered
system image, so nothing a run does reaches a machine anybody cares about.

The emulator's window is centred on a 1024 x 768 display, and has a 4 pixel
border, so the Mac's 512 x 342 screen starts at (260, 217).
"""

import os
import signal
import struct
import subprocess
import time

from PIL import Image

X, Y = 260, 217
W, H = 512, 342

COMMAND, OPTION = "Alt_L", "Super_L"
ROM = "plus-v3"
SYSTEMS = {"6": "system-6.0.8", "7": "system-7.0.1"}
BUILD_TIMEOUT = 300             # macman compiles a build it hasn't got cached


class Mac:
    def __init__(self, workdir, system, disks, prepare=None, display=":77"):
        """prepare, if given, gets the throwaway boot disk's path before the
        machine starts."""
        self.workdir = workdir
        self.display = display
        self.machine = "tico68-emu-%d" % os.getpid()
        self.env = dict(os.environ, DISPLAY=display)
        self.env.pop("WAYLAND_DISPLAY", None)
        os.makedirs(workdir, exist_ok=True)
        self.fb = os.path.join(workdir, "fb")
        os.makedirs(self.fb, exist_ok=True)
        self.xvfb = self.emu = None
        self.log = os.path.join(workdir, "macman.log")
        subprocess.run(["macman", "create", self.machine, "--model", "plus",
                        "--rom", ROM, "--system", SYSTEMS[system],
                        "--memory", "4M", "--scale", "1"],
                       check=True, stdout=subprocess.DEVNULL)
        try:
            if prepare:
                prepare(self.boot_disk())
            self.xvfb = subprocess.Popen(
                ["Xvfb", display, "-screen", "0", "1024x768x24",
                 "-fbdir", self.fb],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                start_new_session=True)
            time.sleep(1)
            run = ["macman", "run", self.machine]
            for disk in disks:
                run += ["--disk", disk]
            with open(self.log, "wb") as log:
                self.emu = subprocess.Popen(
                    run, cwd=workdir, env=self.env, stdout=log,
                    stderr=subprocess.STDOUT, start_new_session=True)
            self.wait_for_window()
        except BaseException:
            self.stop()
            raise

    def boot_disk(self):
        data = os.environ.get("XDG_DATA_HOME") or \
            os.path.expanduser("~/.local/share")
        return os.path.join(data, "macman", "machines", self.machine,
                            "system.dsk")

    def wait_for_window(self):
        """The clock on the boot starts here, not when macman does."""
        deadline = time.time() + BUILD_TIMEOUT
        while time.time() < deadline:
            if self.emu.poll() is not None:
                break
            found = subprocess.run(
                ["xdotool", "search", "--name", "minivmac"], env=self.env,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            if found.returncode == 0:
                return
            time.sleep(0.25)
        with open(self.log) as f:
            raise RuntimeError("no emulator window: " + f.read())

    def quit(self):
        """Mini vMac's own Quit, so that macman sees a clean exit and hasn't
        an abnormal session to warn about on the next run."""
        self.xdo("keydown", "Control_L")
        time.sleep(0.15)
        self.xdo("key", "q")
        time.sleep(0.5)
        self.xdo("key", "y")
        time.sleep(0.15)
        self.xdo("keyup", "Control_L")
        self.emu.wait(timeout=5)

    def stop(self):
        if self.emu and self.emu.poll() is None:
            try:
                self.quit()
            except (OSError, subprocess.SubprocessError):
                pass
        for p in (self.emu, self.xvfb):
            if p is None:
                continue
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            p.wait()
        subprocess.run(["macman", "remove", self.machine, "--delete-disks"],
                       check=True, stdout=subprocess.DEVNULL)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.stop()

    def shot(self):
        """The Mac's screen, as a 1-bit-ish PIL image."""
        with open(os.path.join(self.fb, "Xvfb_screen0"), "rb") as f:
            data = f.read()
        fields = struct.unpack(">25I", data[:100])
        header, width, height = fields[0], fields[4], fields[5]
        line = fields[12]
        ncolors = fields[19]
        pix = data[header + ncolors * 12:]
        img = Image.frombuffer("RGB", (width, height), pix, "raw", "BGRX",
                               line, 1)
        return img.crop((X, Y, X + W, Y + H)).copy()

    def save(self, name):
        path = os.path.join(self.workdir, name + ".png")
        self.shot().resize((W * 2, H * 2), Image.NEAREST).save(path)
        return path

    def xdo(self, *args):
        subprocess.run(["xdotool"] + [str(a) for a in args], env=self.env,
                       check=True)

    def move(self, x, y):
        self.xdo("mousemove", X + x, Y + y)
        time.sleep(0.15)

    def click(self, x, y, repeat=1):
        self.move(x, y)
        self.xdo("mousedown", 1)
        time.sleep(0.1)
        self.xdo("mouseup", 1)
        for _ in range(repeat - 1):
            time.sleep(0.08)
            self.xdo("mousedown", 1)
            time.sleep(0.05)
            self.xdo("mouseup", 1)
        time.sleep(0.2)

    def drag(self, x0, y0, x1, y1, hold=0.2):
        self.move(x0, y0)
        self.xdo("mousedown", 1)
        time.sleep(0.2)
        self.move(x1, y1)
        time.sleep(hold)
        self.xdo("mouseup", 1)
        time.sleep(0.2)

    def menu(self, x, y):
        """Pull down the menu at x, and let go on the item at y."""
        self.drag(x, 8, x, y, hold=0.4)

    def cmd(self, key, *mods):
        """Command-key, with any more modifiers held."""
        for k in (COMMAND,) + mods:
            self.xdo("keydown", k)
        time.sleep(0.15)
        self.xdo("key", key)
        time.sleep(0.15)
        for k in reversed((COMMAND,) + mods):
            self.xdo("keyup", k)
        time.sleep(0.4)

    def type(self, text):
        self.xdo("type", "--delay", 60, text)
        time.sleep(0.3)

    def key(self, *keys):
        self.xdo("key", "--delay", 60, *keys)
        time.sleep(0.3)
