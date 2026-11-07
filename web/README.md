# X-Moto in a web browser

X-Moto compiled to WebAssembly with Emscripten. It runs offline: xmoto.org
sends no CORS headers, so level downloads and highscores are off. Progress is
saved in the browser's IndexedDB.

## Building

You need [emsdk](https://emscripten.org/docs/getting_started/downloads.html),
CMake, Ninja and Python 3. From the repository root:

```
source /path/to/emsdk/emsdk_env.sh
web/build-deps.sh       # once: downloads and builds libxml2
web/build.sh
python3 -m http.server --bind 127.0.0.1 -d build-web/dist 8000
```

Then open http://127.0.0.1:8000/. Opening `index.html` as a file doesn't work.

## Hosting

`build-web/dist/` is static files. The host needs to serve `.wasm` as
`application/wasm`, allow single files above 63 MB (`xmoto.data`), and ideally
compress them (about 70 MB raw, 41 MB gzipped). Keep `SOURCE.txt`,
`COPYING.txt` and `xmoto-source.tar.gz` with the game: the GPL requires the
source to be offered.

File names don't change between builds, so use revalidating cache headers
(ETag or `no-cache`), or browsers may run an old `xmoto.wasm` after an update.

## Resolution

By default the game renders at the window's size when the page loads, and at
least 800x600, since the menus need that much. Options, Video offers "Fit
window" or a fixed size, applied on reload. Whatever the size, the picture is
scaled to fit the window.

`?res=auto` or `?res=1280x720` in the address rewrites the saved resolution,
for when a bad one is stuck. The switch then removes itself from the address.

## Other URL switches

- `?args=a,b,c`: extra command line options, such as `?args=-d`.
- `?exts` or `?exts=novbo`: leave OpenGL extensions on, with or without vertex
  buffers. Both render a black screen under the emulation today.

## How it works

- `shell.html` is the page. It mounts IndexedDB over the game's user
  directories, saves often (a closing tab can't finish saving), chooses the
  resolution and sizes the canvas.
- `pack.py` builds `xmoto.bin` without running the game, which a cross-compile
  can't do.
- `stubs/curl/curl.h` stands in for libcurl. Every transfer fails, so the game
  takes its normal offline paths.
- In the C++ source, Emscripten changes are behind `__EMSCRIPTEN__`. The main
  loop returns to the browser every frame, threads run synchronously, and
  drawing goes through `-sLEGACY_GL_EMULATION`. Physics code is unchanged.
- Input: the browser delivers a frame's key presses at once, so the loop
  replays them at their own times. Each one lands on the 10 ms physics step
  during which it happened, and a tap shorter than a step still counts for
  one step. Gamepads are only read once per frame, so their input is placed
  halfway between reads.
