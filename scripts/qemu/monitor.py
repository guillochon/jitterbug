#!/usr/bin/env python3
"""Screenshot a running QEMU through its monitor socket until QEMU exits.

    monitor.py SOCKET OUTDIR [FAST_SECONDS] [--stimulus]

Takes a PNG every 3 s for the first FAST_SECONDS (default 90, to catch the
startup icon parade), then every 15 s.

--stimulus also types harmless letters into the guest at intervals drawn from
the host CSPRNG, starting 40 s in (after the Finder is up), so the extension's
event filter sees real keyDown events with no one at the keyboard. This tests
the pipeline only: the timing comes from the host, mediated by the emulator,
and is never evidence of OS 9 entropy (CLAUDE.md rule 3).

Runs inside the jitterbug-qemu image (needs pnmtopng from netpbm)."""

import os
import secrets
import socket
import subprocess
import sys
import threading
import time

STIMULUS_START = 40          # seconds after boot
STIMULUS_KEYS = "abcdefghijklmnoprstuvwxyz"   # no 'q': JitterbugTest quits on it


def command(sock_path, line):
    s = socket.socket(socket.AF_UNIX)
    s.connect(sock_path)
    s.settimeout(5)
    try:
        s.recv(4096)                      # banner and prompt
        s.sendall(line.encode() + b"\n")
        time.sleep(0.2)
    finally:
        s.close()


def stimulate(sock_path, stop):
    rng = secrets.SystemRandom()
    sent = 0
    if stop.wait(STIMULUS_START):
        return
    while not stop.is_set():
        try:
            command(sock_path, "sendkey " + rng.choice(STIMULUS_KEYS))
        except OSError:
            break
        sent += 1
        stop.wait(rng.uniform(0.05, 0.6))
    print("monitor: sent %d stimulus keystrokes" % sent)


def main():
    args = [a for a in sys.argv[1:] if a != "--stimulus"]
    sock_path, outdir = args[0], args[1]
    fast = int(args[2]) if len(args) > 2 else 90
    os.makedirs(outdir, exist_ok=True)
    stop = threading.Event()
    if "--stimulus" in sys.argv:
        threading.Thread(target=stimulate, args=(sock_path, stop), daemon=True).start()
    start = time.time()
    n = 0
    while os.path.exists(sock_path):
        t = int(time.time() - start)
        ppm = os.path.join(outdir, "shot-%04d.ppm" % t)
        try:
            command(sock_path, "screendump " + ppm)
        except OSError:
            break                         # QEMU has gone away
        if os.path.exists(ppm):
            with open(ppm[:-4] + ".png", "wb") as png:
                subprocess.run(["pnmtopng", ppm], stdout=png, stderr=subprocess.DEVNULL)
            os.remove(ppm)
            n += 1
        time.sleep(3 if t < fast else 15)
    stop.set()
    print("monitor: %d screenshots in %s" % (n, outdir))


if __name__ == "__main__":
    main()
