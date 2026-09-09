# PROGRESS

## Decisions

| Axis | Choice |
|---|---|
| Artifact | swaying name, reaction-diffusion texture -> char ramp `" .:-=+*#%@"` |
| Interaction | none |
| Build | gcc + gdb + ASan, then port |
| WASM | `clang --target=wasm32 -nostdlib` |
| Design | dark or light page, grayscale, monospace |
| Host | render.com static site |
| Render | fixed logical grid |
| Scope | render first; more changes pending

## Rules

- `src/` allocates nothing. Caller owns the memory.
- `src/` includes `stdint.h`, `stddef.h`, own headers. No libc, no libm.
- Flat buffers, index `y*w + x`. No 2D arrays.
- Time and input arrive as params, never globals.

## Target layout

```
portfolio/
├── Makefile
├── PROGRESS.md
├── src/              # portable C
├── native/           # terminal driver
├── wasm/             # wasm driver: exports, bump allocator, memset/memcpy
├── web/              # index.html, loader JS, waterfall.wasm
└── build/
    ├── native/       # objects + binary
    └── wasm/         # objects
```

## Field model

Gray-Scott reaction-diffusion, two floats per cell, ping-pong buffers:

- nine-point Laplacian: orthogonal weights `wx = k/aspect^2`, `wy = k` for isotropy on screen, diagonals 0.05 to damp checkerboard noise, all summing to 1
- feed and kill blend by the letter mask: inside, moving spots; outside, growth dies back
- 120 iterations per second, `dt` clamped by the driver
- one spore per iteration on a hashed cell inside the mask keeps the letters alive
- mask is caller-filled: canvas text on the web, 5x7 bitmap font in the terminal
- render: the mask is the body at full `@`, `b * GAIN` carves it lighter by up to `DEPTH`, faint bleed outside; the texture fades in over the first seconds
- sway: render samples mask and `b` through a displacement of two sine components across rows, anchored at the bottom, plus a vertical bob; a slow random current scales the amplitude
- the web puts the top three ramp chars on a bright layer

## Stages

### Phase 1: native C

- [x] **t0 toolchain:**
      Makefile, per-target obj dirs, `-MMD -MP`, ASan on compile *and* link
- [x] **t1 grid:**
      Flat `W*H` char buffer, ramp quantization + clamp, static frame to stdout
- [x] **t2 frame loop:**
      ANSI home, absolute-deadline `clock_nanosleep`, measured `dt`
- [x] **t3 field:**
      Integer-hash spawn, column advection -> float per cell -> ramp index
- [x] **t4 struct:**
      `Field` + `field_bytes` / `field_init` / `field_step(dt)`. Static memory, caller-owned

### Phase 2: WASM

- [x] **t5 build:**
      `clang --target=wasm32 -nostdlib -Wl,--no-entry --export=...`. Hand-write `memset` / `memcpy`
- [x] **t6 allocator:**
      Bump allocator over `__heap_base`, `__builtin_wasm_memory_grow`, 64KB pages
- [x] **t7 loader:**
      Instantiate, view `exports.memory` as `Uint8Array`, re-view after any grow
- [x] **t8 render loop:**
      `requestAnimationFrame` -> `step(dt)` -> `subarray` + `TextDecoder` -> `<pre>`

### Phase 3: page

- [x] **t9 mouse:**
      Measure cell size from rendered font, `mousemove` -> segment rasterize -> `cut()`
- [x] **t10 content:**
      Name + info over the field, dark palette, project list scaffold hidden
- [x] **t11 ship:**
      `-Oz` + `--strip-all`, committed `web/waterfall.wasm` (rebuild on C change), `render.yaml` static site

### Phase 4: name

- [x] **t12 reaction:**
      Gray-Scott core, mask API, anisotropic Laplacian, spore
- [x] **t13 mask:**
      canvas text sampled per cell on the web, bitmap font in the terminal
- [ ] **t14 tune:**
      feed / kill inside and out, gain, cell size, settle time by eye
- [ ] **t15 motion:**
      curl-noise drift if the growth alone is too still
- [ ] **t16 demo:**
      regenerate `demo.gif`
