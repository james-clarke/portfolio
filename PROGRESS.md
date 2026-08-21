# PROGRESS

## Decisions

| Axis | Choice |
|---|---|
| Artifact | waterfall field -> char ramp `" .:-=+*#%@"` |
| Interaction | cursor slices the water, flow refills the gap |
| Build | gcc + gdb + ASan, then port |
| WASM | `clang --target=wasm32 -nostdlib` |
| Design | black page, amber accent, monospace |
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

one shift per column tick:

- shift column down one cell, bottom-up
- spawn at top from integer hash, run-length blocks form streams
- per-shift decay (init param, tuned per driver height) -> streams dim as they fall
- cut zeroes cells on the segment; flow from above refills it, no heal logic

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
      `Field` + `field_bytes` / `field_init` / `field_step(dt)` / `field_cut(x0,y0,x1,y1,r)`. Static memory, caller-owned

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
      Name + info over the field, dark/amber palette, project list scaffold hidden
- [ ] **t11 ship:**
      `-Oz`, strip, single static page, `render.yaml` static site
