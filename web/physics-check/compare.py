#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Report where physics runs differ, and how fast they drift apart.

Rides run at 100 steps per second of play, so step / 100 is seconds of play.
"""
import collections
import math
import statistics
import sys

THRESHOLDS_M = [0.01, 1.0]


def load(path):
    rides = collections.defaultdict(list)
    with open(path) as f:
        for line in f:
            p = line.split()
            rides[p[0]].append((p[2], float(p[5]), float(p[6])))
    return rides


def compare(directory, a_name, b_name, label):
    a, b = load(f"{directory}/{a_name}.txt"), load(f"{directory}/{b_name}.txt")
    print(f"\n{label}")
    identical = 0
    seconds = {t: [] for t in THRESHOLDS_M}
    finals = []
    for ride in sorted(a, key=int):
        ra, rb = a[ride], b[ride]
        if all(x[0] == y[0] for x, y in zip(ra, rb)):
            identical += 1
        gaps = [math.dist(x[1:], y[1:]) for x, y in zip(ra, rb)]
        finals.append(gaps[-1])
        for t in THRESHOLDS_M:
            first = next((i for i, g in enumerate(gaps) if g > t), None)
            # rides that never cross count as the full ride length: a lower bound
            seconds[t].append((first if first is not None else len(gaps)) / 100.0)
    n = len(a)
    print(f"  {identical}/{n} rides bit-identical")
    if identical == n:
        return
    for t in THRESHOLDS_M:
        s = seconds[t]
        print(f"  frame gap passes {t:g} m: median {statistics.median(s):6.1f}s, "
              f"earliest {min(s):6.1f}s (ride length {len(next(iter(a.values()))) / 100:.0f}s)")
    print(f"  gap at end of ride: median {statistics.median(finals):.3g} m, worst {max(finals):.3g} m")


def libm(directory):
    print("\nlibm results differing from glibc (out of 20000 per function)")
    differ = collections.Counter()
    with open(f"{directory}/native-libm.txt") as na, open(f"{directory}/wasm-libm.txt") as wa:
        for a, b in zip(na, wa):
            fa, fb = a.split(), b.split()
            if fa[3] != fb[3]:
                differ[fa[0]] += 1
    print("  " + "  ".join(f"{k}:{differ[k]}" for k in
                           ["sin", "cos", "tan", "acos", "asin", "atan2", "pow", "exp", "log", "sqrt"]))


if __name__ == "__main__":
    d = sys.argv[1]
    compare(d, "native-shared", "wasm-shared",
            "A. native vs wasm, SHARED sin/cos/atan2 (is libm the only difference?)")
    compare(d, "native", "wasm",
            "B. native vs wasm, each platform's own libm (what players get)")
    compare(d, "native", "native-retry",
            "C. native vs native, different ODE seed (the original on a retry)")
    compare(d, "native", "native-nofma",
            "D. native vs native, glibc without FMA (the original on an older CPU)")
    libm(d)
