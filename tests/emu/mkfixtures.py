"""Make a data disk for the emulator tests.

Each file is written with hfsutils from a MacBinary, so it can carry the
type, creator, resource fork and flags the tests look at afterwards.

Usage: mkfixtures.py OUT.dsk SCRATCHDIR
"""

import os
import struct
import subprocess
import sys


def crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = (crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def macbinary(name, ftype, creator, data, rsrc=b""):
    h = bytearray(128)
    n = name.encode("mac_roman")
    h[1] = len(n)
    h[2:2 + len(n)] = n
    h[65:69] = ftype
    h[69:73] = creator
    struct.pack_into(">II", h, 83, len(data), len(rsrc))
    h[122] = h[123] = 129
    struct.pack_into(">H", h, 124, crc16(bytes(h[:124])))

    def pad(b):
        return b + bytes(-len(b) % 128)
    return bytes(h) + pad(data) + pad(rsrc)


FILES = {
    "mac.txt": (b"TEXT", b"ttxt", b"mac one\rmac two\rmac three\r", b""),
    "dos.txt": (b"TEXT", b"ttxt", b"dos one\r\ndos two\r\n", b""),
    "unix.txt": (b"TEXT", b"ttxt", b"unix one\nunix two\n", b""),
    "forked": (b"TEXT", b"MSWD", b"has a fork\r", b"RESOURCE FORK BYTES\0\1\2" * 40),
    "locked": (b"TEXT", b"ttxt", b"locked file\r", b""),
    "binary": (b"BINA", b"????", b"data fork of a binary\r", b""),
    "long.txt": (b"TEXT", b"ttxt",
                 b"".join(b"line %d of a longer file\r" % i
                          for i in range(1, 2001)), b""),
}


def main():
    out, scratch = sys.argv[1], sys.argv[2]
    env = dict(os.environ, HOME=scratch)
    os.makedirs(scratch, exist_ok=True)

    def run(*a):
        subprocess.run(a, env=env, check=True, stdout=subprocess.DEVNULL)

    with open(out, "wb") as f:
        f.truncate(800 * 1024)
    run("hformat", "-l", "data", out)
    for name, (ftype, creator, data, rsrc) in FILES.items():
        path = os.path.join(scratch, "file.bin")
        with open(path, "wb") as f:
            f.write(macbinary(name, ftype, creator, data, rsrc))
        run("hcopy", "-m", path, ":" + name)
    run("hattrib", "+l", ":locked")
    run("humount")


if __name__ == "__main__":
    main()
