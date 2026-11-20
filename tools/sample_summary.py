#!/usr/bin/env python3
"""Summarise an OPENCMR2_SAMPLE file: samples per function, with `nm` for the executable.

usage: sample_summary.py samples.bin samples.bin.maps opencmr2 [nm-command] [top]
The executable must be the unstripped one the samples came from; other objects are counted by file name."""
import array
import bisect
import collections
import subprocess
import sys

samples_path, maps_path, exe = sys.argv[1:4]
nm = sys.argv[4] if len(sys.argv) > 4 else "nm"
top = int(sys.argv[5]) if len(sys.argv) > 5 else 40
pcs = array.array("Q")
with open(samples_path, "rb") as f:
    pcs.frombytes(f.read())
regions = []
for line in open(maps_path):
    parts = line.split()
    if len(parts) < 6 or "x" not in parts[1]:
        continue
    lo, hi = (int(x, 16) for x in parts[0].split("-"))
    regions.append((lo, hi, int(parts[2], 16), parts[5]))
exe_name = exe.rsplit("/", 1)[-1]
symbols = []
for line in subprocess.run([nm, "-C", "-n", "--defined-only", exe], capture_output=True, text=True).stdout.splitlines():
    parts = line.split(" ", 2)
    if len(parts) == 3 and parts[1] in "tTwW":
        symbols.append((int(parts[0], 16), parts[2]))
addresses = [a for a, _ in symbols]
counts = collections.Counter()
for pc in pcs:
    for lo, hi, offset, path in regions:
        if lo <= pc < hi:
            if path.endswith(exe_name):
                # PIE: the first executable mapping of the file gives the load base.
                base = min(r[0] - r[2] for r in regions if r[3].endswith(exe_name))
                i = bisect.bisect_right(addresses, pc - base) - 1
                counts[symbols[i][1] if i >= 0 else "?"] += 1
            else:
                counts["[" + path.rsplit("/", 1)[-1] + "]"] += 1
            break
    else:
        counts["[unknown]"] += 1
total = sum(counts.values())
print(f"{total} samples")
for name, n in counts.most_common(top):
    print(f"{100 * n / total:5.1f}%  {n:7d}  {name[:110]}")
