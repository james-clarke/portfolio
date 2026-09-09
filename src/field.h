#ifndef FIELD_H
#define FIELD_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;
    uint32_t seed;
    float age;       /* seconds since init */
    float acc;       /* iterations owed */
    float wx, wy;    /* orthogonal Laplacian weights, isotropic on screen */
    uint32_t iter;   /* iterations done */
    float *a, *b;    /* w*h chemicals */
    float *a2, *b2;  /* w*h back buffers */
    uint8_t *mask;   /* w*h letter coverage, 0..255 */
    char *chars;     /* w*h last rendered frame */
} Field;

/* Memory needed for a w*h field. Constant expression so static buffers
   can be sized exactly at compile time. */
#define FIELD_BYTES(w, h) ((size_t)(w) * (size_t)(h) * (4 * sizeof(float) + 2))

size_t field_bytes(int w, int h);

/* Carve mem (field_bytes(w,h) bytes) into f and reset. aspect is cell
   width over cell height, so diffusion looks the same both ways. */
void field_init(Field *f, int w, int h, uint32_t seed, float aspect, void *mem);

/* Buffer the caller fills with letter coverage, 0..255, row-major. */
uint8_t *field_mask(Field *f);

/* Start the reaction inside the mask. */
void field_seed(Field *f);

/* Advance dt seconds. */
void field_step(Field *f, float dt);

/* Advance to a developed pattern. */
void field_settle(Field *f);

/* Quantize cells to ramp chars, return f->chars (w*h bytes, row-major). */
const char *field_chars(Field *f);

/* Intensity ramp used by field_chars. */
const char *field_ramp(void);

#endif
