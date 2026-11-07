#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Write dist/levels.json, the level list web/level_sweep.js plays through.

Usage: gen_levels.py <xmoto bin dir> <output json>
"""
import json
import pathlib
import re
import sys


def main(bin_dir, out_path):
    levels = []
    root = pathlib.Path(bin_dir)
    for path in sorted((root / "Levels").rglob("*.lvl")):
        text = path.read_text(encoding="utf-8", errors="replace")
        match = re.search(r'<level\s[^>]*\bid="([^"]+)"', text)
        levels.append({
            "id": match.group(1) if match else None,
            "file": path.relative_to(root).as_posix(),
            "chipmunk": 'physics="true"' in text,
        })
    pathlib.Path(out_path).write_text(json.dumps(levels))
    print(f"{len(levels)} levels, {sum(l['chipmunk'] for l in levels)} with Chipmunk, "
          f"{sum(l['id'] is None for l in levels)} without an id")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
