#!/usr/bin/env python3
"""Split a .jbr trace into per-source 8-bit symbol files for NIST's
SP 800-90B tool (ea_non_iid), and print quick per-source statistics.

    tools/jbr2nist.py trace.jbr outdir/
    ea_non_iid -v outdir/timer-jitter.bin 8

The symbol mapping MUST match jb_symbol() in core/jb_engine.c:
  delta-ts sources: low 8 bits of (ts - previous ts of the same source)
  value sources:    low 8 bits of value
The quick stats are sanity checks only - they are NOT min-entropy estimates.
"""
import collections, math, os, struct, sys

def read_trace(path):
    with open(path, "rb") as f:
        if f.read(4) != b"JBR1":
            sys.exit(f"{path}: not a JBR1 trace")
        tb_hz, = struct.unpack(">I", f.read(4))
        names = {}
        for _ in range(f.read(1)[0]):
            sid, kind, ln = f.read(3)
            names[sid] = (f.read(ln).decode("ascii", "replace"), kind)
        recs = []
        while (r := f.read(10)) and len(r) == 10:
            recs.append(struct.unpack(">BBII", r))
    return tb_hz, names, recs

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    tb_hz, names, recs = read_trace(sys.argv[1])
    os.makedirs(sys.argv[2], exist_ok=True)
    syms = collections.defaultdict(bytearray)
    last = {}
    for sid, _flags, ts, val in recs:
        name, kind = names.get(sid, (f"src{sid}", 0))
        if kind == 1:
            syms[name].append(val & 0xFF)
        elif sid in last:
            syms[name].append((ts - last[sid]) & 0xFF)
        last[sid] = ts
    print(f"time base {tb_hz} Hz, {len(recs)} records")
    print(f"{'source':14} {'symbols':>9} {'distinct':>8} {'MCV p':>7} {'naive Hmin':>10}  file")
    for name, data in sorted(syms.items()):
        path = os.path.join(sys.argv[2], f"{name}.bin")
        with open(path, "wb") as f:
            f.write(data)
        c = collections.Counter(data)
        p = c.most_common(1)[0][1] / len(data)
        print(f"{name:14} {len(data):9} {len(c):8} {p:7.4f} {-math.log2(p):10.3f}  {path}")
    print("\nNext: ea_non_iid -v <file> 8   (needs >= 1,000,000 symbols per source for a valid run)")

if __name__ == "__main__":
    main()
