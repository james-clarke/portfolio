#ifndef FIELD_H
#define FIELD_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;
    uint32_t seed;
    float decay;     /* per-shift trail fade; tune to h so streams survive it */
    float *v;        /* w*h cell intensity, 0..1 */
    float *acc;      /* w   column shift accumulator */
    uint32_t *count; /* w   shifts done per column */
    char *chars;     /* w*h last rendered frame */
} Field;

/* Memory needed for a w*h field. Caller allocates, 4-byte aligned. */
size_t field_bytes(int w, int h);

/* Carve mem (field_bytes(w,h) bytes) into f and zero all state. */
void field_init(Field *f, int w, int h, uint32_t seed, float decay, void *mem);

/* Advance columns by dt seconds. */
void field_step(Field *f, float dt);

/* Zero cells within radius r of segment (x0,y0)-(x1,y1). Clips to bounds. */
void field_cut(Field *f, int x0, int y0, int x1, int y1, int r);

/* Quantize cells to ramp chars, return f->chars (w*h bytes, row-major). */
const char *field_chars(Field *f);

#endif
