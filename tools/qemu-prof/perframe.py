#!/usr/bin/env python3
"""Per-frame instruction profile: subtract a 1-frame funcprof run from a 2-frame run.

Work done once (startup, weight loading, initialization) cancels out, leaving the
instructions one extra frame costs: the steady-state cost of inference.

usage: perframe.py prof_1frame.txt prof_2frames.txt [--top N]
"""
import argparse
import re

# "  rank  instructions  %  cum%  function name (may contain spaces)"
ROW = re.compile(r"^\s*\d+\s+(\d+)\s+[\d.]+\s+[\d.]+\s+(.+)$")


def load(path):
    counts = {}
    with open(path) as f:
        for line in f:
            m = ROW.match(line)
            if m:
                counts[m.group(2)] = int(m.group(1))
    return counts


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("one", help="funcprof output for a 1-frame run")
    ap.add_argument("two", help="funcprof output for a 2-frame run")
    ap.add_argument("--top", type=int, default=20, help="rows to print (default 20)")
    args = ap.parse_args()

    one, two = load(args.one), load(args.two)
    delta = {f: two.get(f, 0) - one.get(f, 0) for f in one.keys() | two.keys()}
    shrunk = [f for f, d in delta.items() if d < 0]
    if shrunk:
        print(f"# WARNING: counts went down for {shrunk}: are the two runs comparable?")
    delta = {f: d for f, d in delta.items() if d > 0}
    total = sum(delta.values())

    print(f"# per-frame instructions: {total:,}  (2-frame run minus 1-frame run)")
    print(f"# {'rank':>4} {'instructions':>15} {'%':>7} {'cum%':>7}  function")
    cum = 0.0
    ranked = sorted(delta.items(), key=lambda kv: -kv[1])
    for rank, (func, d) in enumerate(ranked[:args.top], 1):
        pct = 100.0 * d / total
        cum += pct
        print(f"  {rank:>4} {d:>15,} {pct:>7.2f} {cum:>7.2f}  {func}")


if __name__ == "__main__":
    main()
