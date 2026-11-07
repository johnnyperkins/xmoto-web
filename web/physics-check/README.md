# Native vs wasm physics

Does the browser build ride the same as the desktop game? This compares the
vendored ODE built natively (g++) and for wasm (em++) on a bike-shaped world
with the solver settings from `bin/Physics/original.xml`: 20 scripted rides of
167 s, every body's position compared bit for bit at each step. It tests ODE
and libm only, not the game's own bike code (`BikePlayer.cpp`,
`Collision.cpp`), which is unchanged but not compared here.

```
source /path/to/emsdk/emsdk_env.sh
web/physics-check/run.sh        # about 20 s, results in build-physics/
web/physics-check/opt_matrix.sh # does the optimization level matter?
```

## Results

Emscripten 6.0.13 against g++ with glibc 2.43 on x86-64:

- With `sin`, `cos` and `atan2` shared between both builds (`xm_libm.c`),
  native and wasm are bit-identical on all 20 rides, at every optimization
  level. The solver itself is exact in wasm.
- With each platform's own libm, a few percent of `sin`, `cos` and `atan2`
  results differ by one unit in the last place, and the rides drift apart.

Median over 20 rides of the gap between the frames' positions:

| Pair | gap passes 1 cm | gap passes 1 m | gap at end |
|---|---|---|---|
| native vs wasm | 89 s | 122 s | 1.4 m |
| native vs native on a CPU without FMA | 97 s | 132 s | 1.8 m |
| native vs native with another ODE seed, as on a retry | 17 s | 66 s | 6.8 m |

So wasm differs from native about as much as native differs between CPUs, and
less than native differs from itself on a retry: ODE shuffles constraints with
a random generator that is never reset.
