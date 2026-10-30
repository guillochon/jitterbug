#!/usr/bin/env python3
"""Set the Finder's kHasBundle flag on Retro68 application outputs.

Retro68 writes a BNDL resource but never sets the bundle bit, and without it
the Finder ignores the BNDL and shows a generic icon. Run after the
<app>_APPL target (mac/CMakeLists.txt does this):

    mac-set-bundle-bit.py <dir> <AppName>

Patches <AppName>.bin (MacBinary, CRC updated), %<AppName>.ad (AppleDouble),
and <AppName>.dsk (by re-copying the patched .bin with hfsutils, if present).
"""

import os
import shutil
import struct
import subprocess
import sys

K_HAS_BUNDLE = 0x2000


def crc16_xmodem(data):
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def patch_macbinary(path):
    b = bytearray(open(path, "rb").read())
    had_crc = struct.unpack(">H", b[124:126])[0] == crc16_xmodem(b[:124])
    b[73] |= K_HAS_BUNDLE >> 8
    if had_crc:
        b[124:126] = struct.pack(">H", crc16_xmodem(b[:124]))
    open(path, "wb").write(b)


def patch_appledouble(path):
    b = bytearray(open(path, "rb").read())
    if struct.unpack(">I", b[:4])[0] != 0x00051607:
        sys.exit("%s: not AppleDouble" % path)
    (count,) = struct.unpack(">H", b[24:26])
    for i in range(count):
        eid, off, length = struct.unpack(">III", b[26 + 12 * i:38 + 12 * i])
        if eid == 9 and length >= 10:  # Finder info: type, creator, flags
            (flags,) = struct.unpack(">H", b[off + 8:off + 10])
            b[off + 8:off + 10] = struct.pack(">H", flags | K_HAS_BUNDLE)
            open(path, "wb").write(b)
            return
    sys.exit("%s: no Finder info entry" % path)


def patch_disk_image(dsk, binfile, name):
    if not shutil.which("hcopy"):
        print("hfsutils not found; %s keeps the bundle bit clear" % dsk)
        return
    def run(*args):
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL)
    run("hmount", dsk)
    try:
        run("hdel", ":" + name)
        run("hcopy", "-m", binfile, ":" + name)
    finally:
        run("humount")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    d, name = sys.argv[1], sys.argv[2]
    binfile = os.path.join(d, name + ".bin")
    patch_macbinary(binfile)
    ad = os.path.join(d, "%" + name + ".ad")
    if os.path.exists(ad):
        patch_appledouble(ad)
    dsk = os.path.join(d, name + ".dsk")
    if os.path.exists(dsk):
        patch_disk_image(dsk, binfile, name)


if __name__ == "__main__":
    main()
