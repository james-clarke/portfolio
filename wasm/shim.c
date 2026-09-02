#include "field.h"

#define EXPORT(name) __attribute__((export_name(name)))
#define PAGE 65536u

/* linker-provided: first address past data segments */
extern unsigned char __heap_base;

static uintptr_t next;
static Field f;

static void *balloc(size_t n)
{
    uintptr_t p = (next + 15u) & ~(uintptr_t)15u;
    uintptr_t end = p + n;
    size_t have = __builtin_wasm_memory_size(0) * PAGE;

    if (end > have) {
        size_t pages = (end - have + PAGE - 1) / PAGE;
        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1)
            return 0;
    }
    next = end;
    return (void *)p;
}

EXPORT("wf_init")
int wf_init(int w, int h, uint32_t seed, float decay)
{
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
        return -1;
    next = (uintptr_t)&__heap_base;
    void *mem = balloc(field_bytes(w, h));
    if (!mem)
        return -1;
    field_init(&f, w, h, seed, decay, mem);
    return 0;
}

EXPORT("wf_step")
void wf_step(float dt)
{
    field_step(&f, dt);
}

EXPORT("wf_settle")
void wf_settle(void)
{
    field_settle(&f);
}

EXPORT("wf_cut")
void wf_cut(int x0, int y0, int x1, int y1, int r)
{
    field_cut(&f, x0, y0, x1, y1, r);
}

EXPORT("wf_chars")
const char *wf_chars(void)
{
    return field_chars(&f);
}

EXPORT("wf_ramp")
const char *wf_ramp(void)
{
    return field_ramp();
}
