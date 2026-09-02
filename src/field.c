#include "field.h"

static const char RAMP[] = " .:-=+*#%@";
#define RAMP_MAX ((int)sizeof RAMP - 2)

#define RUN 8u           /* spawn decided per 8-cell block */
#define DENSITY 3u       /* 1 in 3 blocks is a stream */
#define SPEED_MIN 2.5f   /* rows per second */
#define SPEED_MAX 11.0f
#define ONSET_S 6.0f     /* seconds from first drop to full rain */
#define ONSET_SPEED 0.35f /* column speed factor at age 0 */

/* lowbias32 finalizer */
static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static uint32_t col_id(uint32_t seed, int x)
{
    return hash32(seed ^ (uint32_t)x * 0x9e3779b9u);
}

static float col_speed(uint32_t seed, int x)
{
    uint32_t r = col_id(seed, x);
    return SPEED_MIN + (SPEED_MAX - SPEED_MIN) * (float)(r & 0xffffu) / 65535.0f;
}

/* 0..1 storm intensity: quadratic ease-out over ONSET_S seconds */
static float onset(float age)
{
    float t = age / ONSET_S;
    if (t >= 1.0f)
        return 1.0f;
    return t * (2.0f - t);
}

/* value entering the top of column x on its n-th shift */
static float spawn(uint32_t seed, int x, uint32_t n, float live)
{
    uint32_t r = hash32(col_id(seed, x) ^ (n / RUN) * 0x85ebca6bu);
    if (r % DENSITY != 0)
        return 0.0f;
    if ((float)(hash32(r) & 0xffffu) > live * 65535.0f)
        return 0.0f;
    return 0.7f + 0.3f * (float)(hash32(r ^ n) & 0xffu) / 255.0f;
}

static void column_shift(Field *f, int x, float head)
{
    float *v = f->v;
    int w = f->w;
    for (int y = f->h - 1; y >= 1; y--)
        v[y * w + x] = v[(y - 1) * w + x] * f->decay;
    v[x] = head;
}

size_t field_bytes(int w, int h)
{
    return FIELD_BYTES(w, h);
}

const char *field_ramp(void)
{
    return RAMP;
}

void field_init(Field *f, int w, int h, uint32_t seed, float decay, void *mem)
{
    size_t n = (size_t)w * (size_t)h;
    unsigned char *p = mem;

    f->w = w;
    f->h = h;
    f->seed = seed;
    f->decay = decay;
    f->age = 0.0f;
    f->v = (float *)p;
    p += n * sizeof(float);
    f->acc = (float *)p;
    p += (size_t)w * sizeof(float);
    f->count = (uint32_t *)p;
    p += (size_t)w * sizeof(uint32_t);
    f->chars = (char *)p;

    for (size_t i = 0; i < n; i++)
        f->v[i] = 0.0f;
    for (int x = 0; x < w; x++) {
        f->acc[x] = 0.0f;
        f->count[x] = 0;
    }
}

void field_step(Field *f, float dt)
{
    float live = onset(f->age);
    float k = ONSET_SPEED + (1.0f - ONSET_SPEED) * live;

    f->age += dt;
    for (int x = 0; x < f->w; x++) {
        f->acc[x] += col_speed(f->seed, x) * k * dt;
        while (f->acc[x] >= 1.0f) {
            f->acc[x] -= 1.0f;
            column_shift(f, x, spawn(f->seed, x, f->count[x]++, live));
        }
    }
}

void field_settle(Field *f)
{
    int n = (int)((ONSET_S + (float)f->h / SPEED_MIN) / 0.1f) + 1;
    while (n--)
        field_step(f, 0.1f);
}

static void stamp(Field *f, int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy > r * r)
                continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= f->w || y < 0 || y >= f->h)
                continue;
            f->v[y * f->w + x] = 0.0f;
        }
}

void field_cut(Field *f, int x0, int y0, int x1, int y1, int r)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        stamp(f, x0, y0, r);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

const char *field_chars(Field *f)
{
    int n = f->w * f->h;
    for (int i = 0; i < n; i++) {
        float x = f->v[i];
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        f->chars[i] = RAMP[(int)(x * RAMP_MAX + 0.5f)];
    }
    return f->chars;
}
