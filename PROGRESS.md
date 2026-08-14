# PROGRESS

## Decisions

| Axis | Choice |
|---|---|
| Artifact | generative field -> char ramp `" .:-=+*#%@"` |
| Build | gcc + gdb + ASan, then port |
| WASM | `clang --target=wasm32 -nostdlib`. |
| Teaching | memory layout, codegen, contracts -> implement |

## Target layout

```
portfolio/
├── Makefile
├── PROGRESS.md
├── src/              # portable C
├── native/           # terminal driver
├── web/              # index.html, loader JS, built .wasm
└── build/            # objects + binaries
```

## Stages

### Phase 1: native C

- [ ] **t0 toolchain:**
      Makefile
- [ ] **t1 grid:**
      Fixed `W*H` char buffer, ramp quantization, static frame to stdout
- [ ] **t2 frame loop:**
      ANSI clear, `nanosleep`, fixed fps
- [ ] **t3 field:**
      Noise -> float per cell → ramp index
- [ ] **t4 heap:**
      Terminal size query, `malloc`'d grid, SIGWINCH, `free`
- [ ] **t5 struct:**
      `Field` struct + `field_init` / `field_step` / `field_destroy`.

### Phase 2: WASM

- [ ] **t6 build:**
      `clang --target=wasm32 -nostdlib -Wl,--no-entry --export=...`
- [ ] **t7 allocator:**
      Bump allocator over `__heap_base`, `__builtin_wasm_memory_grow`, 64KB pages
- [ ] **t8 javascript**
      Instantiate, view `exports.memory` as `Uint8Array`, read grid in place
- [ ] **t9 render loop:**
      `requestAnimationFrame` -> `step()` -> paint into `<pre>`

### Phase 3

- [ ] **t10 input:** — mouse/scroll into wasm, tunable field params.
- [ ] **t11 ship:** — `-Oz`, strip, single static HTML page, deploy.

