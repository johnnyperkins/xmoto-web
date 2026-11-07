#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds libxml2 for Emscripten, which has no port of it, into
# build-web/deps/prefix. Needs emcmake, cmake and ninja on PATH.
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${XM_WEB_BUILD_DIR:-$SRC/build-web}/deps"
VER=2.13.9
SHA256=a2c9ae7b770da34860050c309f903221c67830c86e4a7e760692b803df95143a
TARBALL="libxml2-$VER.tar.xz"
URL="https://download.gnome.org/sources/libxml2/${VER%.*}/$TARBALL"

mkdir -p "$OUT"
cd "$OUT"
if [ ! -f "$TARBALL" ]; then
  curl -fsSL -o "$TARBALL.part" "$URL"
  mv "$TARBALL.part" "$TARBALL"
fi
if command -v sha256sum >/dev/null; then
  echo "$SHA256  $TARBALL" | sha256sum -c -
else
  echo "$SHA256  $TARBALL" | shasum -a 256 -c -
fi

rm -rf "libxml2-$VER" libxml2-build
tar -xf "$TARBALL"
emcmake cmake -S "libxml2-$VER" -B libxml2-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$OUT/prefix" \
  -DCMAKE_C_FLAGS=-fexceptions \
  -DBUILD_SHARED_LIBS=OFF \
  -DLIBXML2_WITH_PYTHON=OFF -DLIBXML2_WITH_THREADS=OFF \
  -DLIBXML2_WITH_ICONV=OFF -DLIBXML2_WITH_ICU=OFF \
  -DLIBXML2_WITH_LZMA=OFF -DLIBXML2_WITH_ZLIB=OFF \
  -DLIBXML2_WITH_HTTP=OFF -DLIBXML2_WITH_FTP=OFF \
  -DLIBXML2_WITH_PROGRAMS=OFF -DLIBXML2_WITH_TESTS=OFF
cmake --build libxml2-build
cmake --install libxml2-build
