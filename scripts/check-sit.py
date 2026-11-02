#!/usr/bin/env python3
"""Check tico68.sit with The Unarchiver's lsar and unar, which share no code
with sit, against the files it was made from.

Usage: check-sit.py BUILD_DIR
"""

import json
import os
import struct
import subprocess
import sys
import tempfile

HAS_BUNDLE = 0x2000
failures = []


def check(name, ok, detail=""):
    print(("ok   " if ok else "FAIL ") + name + (" " + detail if detail else ""))
    if not ok:
        failures.append(name)


def ostype(s):
    return struct.unpack(">I", s)[0]


def read(path):
    with open(path, "rb") as f:
        return f.read()


def appledouble_rsrc(path):
    """unar writes a resource fork as an AppleDouble file: find the fork."""
    b = read(path)
    for i in range(struct.unpack(">H", b[24:26])[0]):
        entry, offset, length = struct.unpack(">III", b[26 + i * 12:38 + i * 12])
        if entry == 2:
            return b[offset:offset + length]
    return b""


def main():
    build = sys.argv[1]
    sit = os.path.join(build, "tico68.sit")
    binary = read(os.path.join(build, "tico68.bin"))
    readme = read(os.path.join(build, "ReadMe.txt"))
    dlen, rlen = struct.unpack(">II", binary[83:91])
    rsrc = binary[128 + dlen + (-dlen % 128):][:rlen]

    check("archive is classic StuffIt, not the 1997 format",
          read(sit)[:4] == b"SIT!" and read(sit)[10:14] == b"rLau")

    listing = json.loads(subprocess.run(["lsar", "-j", sit], check=True,
                                        capture_output=True).stdout)
    forks = listing["lsarContents"]
    names = sorted(e["XADFileName"] for e in forks)
    check("holds exactly tico68 and ReadMe.txt",
          names == ["ReadMe.txt", "tico68"], repr(names))
    if failures:
        print(len(failures), "failed:", ", ".join(failures))
        return 1

    app = next(e for e in forks if e["XADFileName"] == "tico68")
    check("tico68 is the resource fork", app.get("XADIsResourceFork") == 1)
    check("tico68 is APPL/Tc68",
          app["XADFileType"] == ostype(b"APPL") and
          app["XADFileCreator"] == ostype(b"Tc68"))
    check("tico68 has its bundle bit", app["XADFinderFlags"] & HAS_BUNDLE != 0)

    text = next(e for e in forks if e["XADFileName"] == "ReadMe.txt")
    check("ReadMe.txt is TEXT/Tc68",
          text["XADFileType"] == ostype(b"TEXT") and
          text["XADFileCreator"] == ostype(b"Tc68"))

    with tempfile.TemporaryDirectory() as out:
        subprocess.run(["unar", "-q", "-k", "visible", "-D", "-o", out, sit],
                       check=True)
        check("resource fork matches tico68.bin",
              appledouble_rsrc(os.path.join(out, "tico68.rsrc")) == rsrc)
        check("ReadMe.txt matches", read(os.path.join(out, "ReadMe.txt")) == readme)
        check("ReadMe.txt has CR line endings",
              b"\r" in readme and b"\n" not in readme)

    if failures:
        print(len(failures), "failed:", ", ".join(failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
