#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Compares native (g++) and wasm (em++) builds of the vendored ODE on one scripted
# scenario, with each platform's own libm and with a shared one, and compares native
# against itself with a different ODE seed. Needs g++, and emsdk on PATH (source
# emsdk_env.sh), which also provides node. No game build needed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" # repository root
HERE="$ROOT/web/physics-check"
OUT="$ROOT/build-physics"
ODE="$ROOT/vendor/ode"
CCD="$ROOT/vendor/libccd"
EMXX=em++
EMCC=emcc
SHARED_MATH="-fno-builtin-sin -fno-builtin-cos -fno-builtin-atan2"

# Same optimisation as an upstream CMake Release build, no -march flags.
BASE="-O3 -DNDEBUG -DdDOUBLE -w"
INC="-I$ODE/include -I$ODE/src -I$ODE/src/joints -I$CCD/include -I$CCD/src"
SOURCES="$(grep -oE 'src/[A-Za-z0-9_/]+\.(cpp|c)\b' "$ODE/CMakeLists.txt" | sort -u)"

# compile_ode <object dir> <c compiler> <c++ compiler> <extra flags>
compile_ode() {
  local dir="$1" cc="$2" cxx="$3" extra="$4"
  mkdir -p "$dir"
  for s in $SOURCES; do
    local obj="$dir/$(echo "$s" | tr '/' '_').o"
    [ -f "$obj" ] && continue
    case "$s" in
      *.cpp) echo "$cxx -std=gnu++11 $BASE $extra $INC -c $ODE/$s -o $obj" ;;
      *) echo "$cc $BASE $extra $INC -c $ODE/$s -o $obj" ;;
    esac
  done | xargs -P "$(nproc)" -I{} sh -c '{}'
  for s in "$CCD"/src/*.c; do
    local obj="$dir/ccd_$(basename "$s").o"
    [ -f "$obj" ] || $cc $BASE $extra $INC -c "$s" -o "$obj"
  done
}

compile_ode "$OUT/obj-native" gcc g++ "-fPIC"
compile_ode "$OUT/obj-wasm" "$EMCC" "$EMXX" "-fexceptions"
compile_ode "$OUT/obj-native-shared" gcc g++ "-fPIC $SHARED_MATH"
compile_ode "$OUT/obj-wasm-shared" "$EMCC" "$EMXX" "-fexceptions $SHARED_MATH"

WASMFLAGS="-sENVIRONMENT=node -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8MB"

g++ -std=gnu++11 $BASE $INC "$HERE/scenario.cpp" "$OUT"/obj-native/*.o -lm -o "$OUT/native"
"$EMXX" -std=gnu++11 $BASE -fexceptions $INC "$HERE/scenario.cpp" "$OUT"/obj-wasm/*.o $WASMFLAGS -o "$OUT/wasm.js"

gcc $BASE $SHARED_MATH -c "$HERE/xm_libm.c" -o "$OUT/xm_libm-native.o"
"$EMCC" $BASE $SHARED_MATH -fexceptions -c "$HERE/xm_libm.c" -o "$OUT/xm_libm-wasm.o"
g++ -std=gnu++11 $BASE $SHARED_MATH $INC "$HERE/scenario.cpp" "$OUT"/obj-native-shared/*.o "$OUT/xm_libm-native.o" -lm -o "$OUT/native-shared"
"$EMXX" -std=gnu++11 $BASE -fexceptions $SHARED_MATH $INC "$HERE/scenario.cpp" "$OUT"/obj-wasm-shared/*.o "$OUT/xm_libm-wasm.o" $WASMFLAGS -o "$OUT/wasm-shared.js"

cd "$OUT"
./native physics 0 > native.txt
./native physics 123456789 > native-retry.txt
GLIBC_TUNABLES="glibc.cpu.hwcaps=-FMA,-AVX2,-FMA4" ./native physics 0 > native-nofma.txt
node wasm.js physics 0 > wasm.txt
./native-shared physics 0 > native-shared.txt
node wasm-shared.js physics 0 > wasm-shared.txt
./native libm > native-libm.txt
node wasm.js libm > wasm-libm.txt

python3 -I "$HERE/compare.py" "$OUT"
