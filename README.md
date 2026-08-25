# portfolio

![ascii rain in a terminal](demo.gif)

## layout

```
src/      core: no libc, no libm, no allocator
native/   terminal driver
wasm/     freestanding driver
web/      static site
```

## build

```
make native   # gcc + ASan
make wasm     # clang --target=wasm32 -nostdlib
```

`web/waterfall.wasm` is committed, you have to rebuild it when `src/` or `wasm/` changes.
