"""Drive Mini vMac headlessly: Xvfb for a display, xdotool for input, and
Xvfb's framebuffer file for screenshots.

The emulator's screen is 512 x 342 at (256, 213) on a 1024 x 768 display.
"""

import os
import shutil
import signal
import struct
import subprocess
import time

from PIL import Image

X, Y = 256, 213
W, H = 512, 342


class Mac:
    def __init__(self, workdir, disks, display=":77", speed_note=None):
        self.workdir = workdir
        self.display = display
        # SDL prefers Wayland when it can, which would be the user's desktop
        self.env = dict(os.environ, DISPLAY=display, SDL_AUDIODRIVER="dummy",
                        SDL_VIDEO_DRIVER="x11", SDL_VIDEODRIVER="x11")
        self.env.pop("WAYLAND_DISPLAY", None)
        os.makedirs(workdir, exist_ok=True)
        self.fb = os.path.join(workdir, "fb")
        os.makedirs(self.fb, exist_ok=True)
        run = os.path.expanduser("~/opt/minivmac/git/run")
        shutil.copy(os.path.join(run, "vMac.ROM"),
                    os.path.join(workdir, "vMac.ROM"))
        self.xvfb = subprocess.Popen(
            ["Xvfb", display, "-screen", "0", "1024x768x24", "-fbdir", self.fb],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True)
        time.sleep(1)
        self.emu = subprocess.Popen(
            [os.path.join(run, "minivmac")] + disks, cwd=workdir, env=self.env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True)

    def stop(self):
        for p in (self.emu, self.xvfb):
            try:
                os.killpg(p.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            p.wait()

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
        """Command-key: Super is Command here, Alt is Option."""
        for k in ("Super_L",) + mods:
            self.xdo("keydown", k)
        time.sleep(0.15)
        self.xdo("key", key)
        time.sleep(0.15)
        for k in reversed(("Super_L",) + mods):
            self.xdo("keyup", k)
        time.sleep(0.4)

    def type(self, text):
        self.xdo("type", "--delay", 60, text)
        time.sleep(0.3)

    def key(self, *keys):
        self.xdo("key", "--delay", 60, *keys)
        time.sleep(0.3)
