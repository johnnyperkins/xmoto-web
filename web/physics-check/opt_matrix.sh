#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Does the optimisation level change ODE's results? Builds with shared sin/cos at
# several levels, native and wasm, then reports the first differing step per pair.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" # repository root
HERE="$ROOT/web/physics-check"
OUT="$ROOT/build-physics/matrix"
ODE="$ROOT/vendor/ode"
CCD="$ROOT/vendor/libccd"
EMXX=em++
EMCC=emcc
COMMON="-DNDEBUG -DdDOUBLE -w -fno-builtin-sin -fno-builtin-cos -fno-builtin-atan2"
INC="-I$ODE/include -I$ODE/src -I$ODE/src/joints -I$CCD/include -I$CCD/src"
SOURCES="$(grep -oE 'src/[A-Za-z0-9_/]+\.(cpp|c)\b' "$ODE/CMakeLists.txt" | sort -u)"
WASMFLAGS="-sENVIRONMENT=node -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=8MB"

# build <name> <cc> <cxx> <flags> <link extra>
build() {
  local name="$1" cc="$2" cxx="$3" flags="$4" link="$5" ext="$6"
  local dir="$OUT/obj-$name"
  mkdir -p "$dir"
  for s in $SOURCES; do
    local obj="$dir/$(echo "$s" | tr '/' '_').o"
    case "$s" in
      *.cpp) echo "$cxx -std=gnu++11 $COMMON $flags $INC -c $ODE/$s -o $obj" ;;
      *) echo "$cc $COMMON $flags $INC -c $ODE/$s -o $obj" ;;
    esac
  done | xargs -P "$(nproc)" -I{} sh -c '{}'
  for s in "$CCD"/src/*.c; do
    $cc $COMMON $flags $INC -c "$s" -o "$dir/ccd_$(basename "$s").o"
  done
  $cc $COMMON $flags -c "$HERE/xm_libm.c" -o "$dir/xm_libm.o"
  $cxx -std=gnu++11 $COMMON $flags $INC "$HERE/scenario.cpp" "$dir"/*.o $link -o "$OUT/$name$ext"
}

build native-O0 gcc g++ "-O0 -fPIC" "-lm" ""
build native-O2 gcc g++ "-O2 -fPIC" "-lm" ""
build native-O3 gcc g++ "-O3 -fPIC" "-lm" ""
build wasm-O0 "$EMCC" "$EMXX" "-O0 -fexceptions" "$WASMFLAGS" ".js"
build wasm-O2 "$EMCC" "$EMXX" "-O2 -fexceptions" "$WASMFLAGS" ".js"
build wasm-O3 "$EMCC" "$EMXX" "-O3 -fexceptions" "$WASMFLAGS" ".js"

cd "$OUT"
for n in native-O0 native-O2 native-O3; do ./"$n" physics 0 > "$n.txt"; done
for n in wasm-O0 wasm-O2 wasm-O3; do node "$n.js" physics 0 > "$n.txt"; done

python3 -I - <<'EOF'
import itertools
names = ["native-O0", "native-O2", "native-O3", "wasm-O0", "wasm-O2", "wasm-O3"]
runs = {n: [l.split()[2] for l in open(f"{n}.txt")] for n in names}
for a, b in itertools.combinations(names, 2):
    first = next((i for i, (x, y) in enumerate(zip(runs[a], runs[b])) if x != y), None)
    same = sum(x == y for x, y in zip(runs[a], runs[b]))
    verdict = "IDENTICAL" if first is None else f"differs from step {first}"
    print(f"{a:10s} vs {b:10s}  {verdict:24s} {same}/{len(runs[a])} steps match")
EOF
