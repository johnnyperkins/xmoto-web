#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds X-Moto for the browser. Needs emcc, cmake and ninja on PATH (source
# emsdk_env.sh first) and libxml2 from web/build-deps.sh.
#   build-web/cmake/   CMake build tree
#   build-web/dist/    the files to host
# XM_WEB_DEBUG=1 keeps function names in the wasm, for readable stack traces.
set -euo pipefail

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${XM_WEB_BUILD_DIR:-$SRC/build-web}"
DIST="$OUT/dist"
LIBXML2="${XM_WEB_LIBXML2_PREFIX:-$OUT/deps/prefix}"

mkdir -p "$OUT"
python3 -I "$SRC/web/pack.py" "$SRC/bin" "$OUT/xmoto.bin"

# No -ffast-math: physics must keep IEEE semantics.
PORTS="-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2 -sUSE_SDL_NET=2 -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_ZLIB=1 -sUSE_SQLITE3=1 -sSDL2_MIXER_FORMATS=[\"ogg\"]"
CFLAGS="$PORTS -fexceptions -Wno-c++11-narrowing -Wno-register -Wno-deprecated-declarations"

DEBUG_FLAGS=""
if [ "${XM_WEB_DEBUG:-0}" = "1" ]; then DEBUG_FLAGS="--profiling-funcs"; fi

LDFLAGS="$PORTS -fexceptions -lGL -lidbfs.js $DEBUG_FLAGS
  -sLEGACY_GL_EMULATION=1
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sSTACK_SIZE=8MB
  -sEXIT_RUNTIME=0 -sFORCE_FILESYSTEM=1
  -sEXPORTED_RUNTIME_METHODS=[\"FS\",\"IDBFS\",\"addRunDependency\",\"removeRunDependency\",\"UTF8ToString\",\"ccall\"]
  --shell-file $SRC/web/shell.html
  --preload-file $OUT/xmoto.bin@/usr/share/xmoto/xmoto.bin
  --preload-file $SRC/bin/Textures/Fonts@/usr/share/xmoto/Textures/Fonts
  --preload-file $SRC/bin/Textures/Musics@/usr/share/xmoto/Textures/Musics"
LDFLAGS="$(echo "$LDFLAGS" | tr -s ' \n' ' ')"

emcmake cmake -S "$SRC" -B "$OUT/cmake" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$CFLAGS" -DCMAKE_CXX_FLAGS="$CFLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
  -DUSE_GETTEXT=OFF -DPREFER_SYSTEM_Lua=OFF -DPREFER_SYSTEM_BZip2=OFF -DPREFER_SYSTEM_XDG=OFF \
  -DXM_WEB_LIBXML2_PREFIX="$LIBXML2"
cmake --build "$OUT/cmake" "$@"

rm -rf "$DIST"
mkdir -p "$DIST"
cp "$OUT/cmake/src/xmoto.html" "$DIST/index.html"
cp "$OUT/cmake/src/xmoto.js" "$OUT/cmake/src/xmoto.wasm" "$OUT/cmake/src/xmoto.data" "$DIST/"
cp "$SRC/COPYING" "$DIST/COPYING.txt"

# GPL: ship the source of exactly this build. Game data (bin/) is left out to
# keep it small; it is unchanged from the repository.
if ! git -C "$SRC" rev-parse --git-dir >/dev/null 2>&1; then
  echo "error: $SRC is not a git checkout, can't bundle the source" >&2
  exit 1
fi
COMMIT="$(git -C "$SRC" rev-parse HEAD)"
TREE="$(git -C "$SRC" stash create)" # HEAD plus changes to tracked files
if [ -n "$TREE" ]; then
  COMMIT="$COMMIT plus local changes"
fi
if [ -n "$(git -C "$SRC" ls-files --others --exclude-standard)" ]; then
  echo "warning: untracked files are not in the source archive" >&2
fi
git -C "$SRC" archive --format=tar.gz --prefix=xmoto/ -o "$DIST/xmoto-source.tar.gz" "${TREE:-HEAD}" -- . ':(exclude)bin'
sed "s|@COMMIT@|$COMMIT|" "$SRC/web/SOURCE.txt.in" > "$DIST/SOURCE.txt"

echo "Serve with: python3 -m http.server --bind 127.0.0.1 -d $DIST 8000"
