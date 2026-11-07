#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build xmoto.bin (XBI3) without running xmoto.

Same layout as Packager::go in src/common/Packager.cpp. The upstream build
runs `xmoto --pack`, which can't run when cross-compiling to wasm.

Usage: pack.py <data dir containing package.lst> <output file>
"""
import hashlib
import pathlib
import struct
import sys


def md5(path):
    return hashlib.md5(path.read_bytes()).hexdigest().encode()


def main(data_dir, out_path):
    data_dir = pathlib.Path(data_dir)
    pkglist = data_dir / "package.lst"
    names = [l.strip() for l in pkglist.read_text().splitlines() if l.strip()]

    missing = 0
    with open(out_path, "wb") as out:
        out.write(b"XBI3")
        sum_ = md5(pkglist)
        out.write(bytes([len(sum_)]) + sum_)
        for name in names:
            path = data_dir / name
            if not path.is_file():
                print(f"!!! FAILED TO ADD {name!r}", file=sys.stderr)
                missing += 1
                continue
            encoded = name.encode()
            digest = md5(path)
            body = path.read_bytes()
            out.write(bytes([len(encoded)]) + encoded)
            out.write(bytes([len(digest)]) + digest)
            out.write(struct.pack("<i", len(body)))
            out.write(body)
    print(f"packed {len(names) - missing} files, {missing} missing")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
