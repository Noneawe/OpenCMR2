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
words = array.array("Q")
with open(samples_path, "rb") as f:
    words.frombytes(f.read())
pcs, callers = words[0::2], words[1::2]
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
via = collections.defaultdict(collections.Counter)   # a library -> the functions of the program that called it
base = min(r[0] - r[2] for r in regions if r[3].endswith(exe_name))
def where(pc):
    for lo, hi, offset, path in regions:
        if lo <= pc < hi:
            if path.endswith(exe_name):
                i = bisect.bisect_right(addresses, pc - base) - 1
                return symbols[i][1] if i >= 0 else "?", True
            return "[" + path.rsplit("/", 1)[-1] + "]", False
    return "[unknown]", False
for pc, caller in zip(pcs, callers):
    name, inside = where(pc)
    counts[name] += 1
    if not inside:
        who, in_exe = where(caller)
        via[name][who] += 1
total = sum(counts.values())
print(f"{total} samples")
for name, n in counts.most_common(top):
    print(f"{100 * n / total:5.1f}%  {n:7d}  {name[:110]}")

for lib in ("[libc-2.30.so]", "[libmali-bifrost-g31-rxp0-gbm.so]"):
    if lib in via:
        print(f"\ncalled from, for {lib} ({sum(via[lib].values())} samples; the link register: a leaf's caller):")
        for name, n in via[lib].most_common(12):
            print(f"{100 * n / total:5.1f}%  {n:7d}  {name[:110]}")
