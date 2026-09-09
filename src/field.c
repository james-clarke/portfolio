#include "field.h"

static const char RAMP[] = " .:-=+*#%@";
#define RAMP_MAX ((int)sizeof RAMP - 2)

#define RATE 120.0f      /* iterations per second */
#define SETTLE_S 20.0f
#define DA 1.0f
#define DB 0.5f
#define F_IN 0.010f      /* feed inside the letters: turbulent waves */
#define K_IN 0.047f
#define F_OUT 0.010f     /* feed outside: growth dies back */
#define K_OUT 0.062f
#define GAIN 2.5f        /* b to texture intensity */
#define DEPTH 0.3f       /* how far the texture carves into the `@` body */
#define BLEED 0.3f       /* texture intensity outside the body */
#define TEXTURE_S 8.0f   /* seconds for the texture to fade in */
#define WD 0.05f         /* diagonal Laplacian weight, damps checkerboard noise */
#define SWAY_X1 1.8f     /* cells, slow component */
#define SWAY_T1 11.0f    /* seconds per cycle */
#define SWAY_L1 40.0f    /* rows per cycle */
#define SWAY_X2 0.5f     /* cells, fast component */
#define SWAY_T2 7.0f
#define SWAY_L2 17.0f
#define SWAY_Y 0.25f     /* rows, vertical bob */
#define SWAY_T3 8.0f
#define SWAY_L3 30.0f    /* columns per cycle */
#define CURRENT_S 20.0f  /* seconds between current levels */

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

/* 0..1 level for period k of a slow random walk */
static float level(uint32_t seed, uint32_t salt, int k)
{
    return (float)(hash32(seed ^ salt ^ (uint32_t)k * 0x27d4eb2fu) & 0xffffu) / 65535.0f;
}

/* smoothstep between successive levels, one per period seconds */
static float drift(uint32_t seed, uint32_t salt, float age, float period)
{
    int k = (int)(age / period);
    float t = age / period - (float)k;
    float a = level(seed, salt, k), b = level(seed, salt, k + 1);
    t = t * t * (3.0f - 2.0f * t);
    return a + (b - a) * t;
}

/* sine of t turns, Bhaskara's approximation, error under 0.002 */
static float sin1(float t)
{
    float s = 1.0f, u, q;
    t -= (float)(int)t;
    if (t < 0.0f)
        t += 1.0f;
    if (t >= 0.5f) {
        t -= 0.5f;
        s = -1.0f;
    }
    u = t * (0.5f - t);
    q = 64.0f * u / (5.0f - 16.0f * u);
    return s * q;
}

/* bilinear sample of a w*h float grid, clamped */
static float sample_f(const float *g, int w, int h, float x, float y)
{
    int x0, y0, x1, y1;
    float fx, fy, top, bot;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > (float)(w - 1)) x = (float)(w - 1);
    if (y > (float)(h - 1)) y = (float)(h - 1);
    x0 = (int)x;
    y0 = (int)y;
    fx = x - (float)x0;
    fy = y - (float)y0;
    x1 = x0 < w - 1 ? x0 + 1 : x0;
    y1 = y0 < h - 1 ? y0 + 1 : y0;
    top = g[y0 * w + x0] + (g[y0 * w + x1] - g[y0 * w + x0]) * fx;
    bot = g[y1 * w + x0] + (g[y1 * w + x1] - g[y1 * w + x0]) * fx;
    return top + (bot - top) * fy;
}

static float sample_u8(const uint8_t *g, int w, int h, float x, float y)
{
    int x0, y0, x1, y1;
    float fx, fy, top, bot;
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > (float)(w - 1)) x = (float)(w - 1);
    if (y > (float)(h - 1)) y = (float)(h - 1);
    x0 = (int)x;
    y0 = (int)y;
    fx = x - (float)x0;
    fy = y - (float)y0;
    x1 = x0 < w - 1 ? x0 + 1 : x0;
    y1 = y0 < h - 1 ? y0 + 1 : y0;
    top = (float)g[y0 * w + x0] + ((float)g[y0 * w + x1] - (float)g[y0 * w + x0]) * fx;
    bot = (float)g[y1 * w + x0] + ((float)g[y1 * w + x1] - (float)g[y1 * w + x0]) * fx;
    return (top + (bot - top) * fy) * (1.0f / 255.0f);
}

static void iterate(Field *f)
{
    int w = f->w, h = f->h;
    const float *a = f->a, *b = f->b;
    float *a2 = f->a2, *b2 = f->b2;
    const uint8_t *mask = f->mask;
    float wx = f->wx, wy = f->wy;

    for (int y = 0; y < h; y++) {
        int up = (y > 0 ? y - 1 : y) * w;
        int dn = (y < h - 1 ? y + 1 : y) * w;
        int row = y * w;
        for (int x = 0; x < w; x++) {
            int i = row + x;
            int l = x > 0 ? x - 1 : x;
            int r = x < w - 1 ? x + 1 : x;
            float m = (float)mask[i] * (1.0f / 255.0f);
            float F = F_OUT + (F_IN - F_OUT) * m;
            float K = K_OUT + (K_IN - K_OUT) * m;
            float ca = a[i], cb = b[i];
            float la = wx * (a[row + l] + a[row + r] - 2.0f * ca)
                     + wy * (a[up + x] + a[dn + x] - 2.0f * ca)
                     + WD * (a[up + l] + a[up + r] + a[dn + l] + a[dn + r] - 4.0f * ca);
            float lb = wx * (b[row + l] + b[row + r] - 2.0f * cb)
                     + wy * (b[up + x] + b[dn + x] - 2.0f * cb)
                     + WD * (b[up + l] + b[up + r] + b[dn + l] + b[dn + r] - 4.0f * cb);
            float abb = ca * cb * cb;
            float na = ca + DA * la - abb + F * (1.0f - ca);
            float nb = cb + DB * lb + abb - (F + K) * cb;
            a2[i] = na < 0.0f ? 0.0f : na > 1.0f ? 1.0f : na;
            b2[i] = nb < 0.0f ? 0.0f : nb > 1.0f ? 1.0f : nb;
        }
    }
    f->a = a2;
    f->b = b2;
    f->a2 = (float *)a;
    f->b2 = (float *)b;

    /* a spore keeps the letters alive if a region dies out */
    {
        uint32_t i = hash32(f->seed ^ f->iter++ * 0x9e3779b9u) % (uint32_t)(w * h);
        if (mask[i] > 127)
            f->b[i] = 1.0f;
    }
}

size_t field_bytes(int w, int h)
{
    return FIELD_BYTES(w, h);
}

const char *field_ramp(void)
{
    return RAMP;
}

void field_init(Field *f, int w, int h, uint32_t seed, float aspect, void *mem)
{
    size_t n = (size_t)w * (size_t)h;
    unsigned char *p = mem;
    float inv = 1.0f / (aspect * aspect);
    float k = (1.0f - 4.0f * WD) / (2.0f * (1.0f + inv)); /* all nine weights sum to 1 */

    f->w = w;
    f->h = h;
    f->seed = seed;
    f->age = 0.0f;
    f->acc = 0.0f;
    f->wx = k * inv;
    f->wy = k;
    f->iter = 0;
    f->a = (float *)p;
    p += n * sizeof(float);
    f->b = (float *)p;
    p += n * sizeof(float);
    f->a2 = (float *)p;
    p += n * sizeof(float);
    f->b2 = (float *)p;
    p += n * sizeof(float);
    f->mask = (uint8_t *)p;
    p += n;
    f->chars = (char *)p;

    for (size_t i = 0; i < n; i++) {
        f->a[i] = 1.0f;
        f->b[i] = 0.0f;
        f->mask[i] = 0;
    }
}

uint8_t *field_mask(Field *f)
{
    return f->mask;
}

void field_seed(Field *f)
{
    size_t n = (size_t)f->w * (size_t)f->h;

    for (size_t i = 0; i < n; i++) {
        float m = (float)f->mask[i] / 255.0f;
        float r = (float)(hash32(f->seed ^ (uint32_t)i * 0x85ebca6bu) & 0xffu) / 255.0f;
        f->b[i] = m * (0.5f + 0.5f * r);
        f->a[i] = 1.0f - f->b[i];
    }
}

void field_step(Field *f, float dt)
{
    f->age += dt;
    f->acc += RATE * dt;
    while (f->acc >= 1.0f) {
        f->acc -= 1.0f;
        iterate(f);
    }
}

void field_settle(Field *f)
{
    int n = (int)(SETTLE_S / 0.1f);
    while (n--)
        field_step(f, 0.1f);
}

/* the body and its texture, swayed by the current and quantized */
const char *field_chars(Field *f)
{
    int w = f->w, h = f->h;
    float t = f->age;
    float current = 0.5f + 0.6f * drift(f->seed, 0x2545f491u, t, CURRENT_S);
    float texture = t < TEXTURE_S ? t / TEXTURE_S : 1.0f;

    for (int y = 0; y < h; y++) {
        float anchor = h > 1 ? 1.0f - (float)y / (float)(h - 1) : 0.0f; /* top sways most */
        float dx = current * anchor
                 * (SWAY_X1 * sin1(t / SWAY_T1 + (float)y / SWAY_L1)
                  + SWAY_X2 * sin1(t / SWAY_T2 + (float)y / SWAY_L2));
        for (int x = 0; x < w; x++) {
            float dy = current * SWAY_Y * sin1(t / SWAY_T3 + (float)x / SWAY_L3);
            float sx = (float)x - dx, sy = (float)y - dy;
            float m = sample_u8(f->mask, w, h, sx, sy);
            float p = sample_f(f->b, w, h, sx, sy) * GAIN * texture;
            float v;
            if (p > 1.0f) p = 1.0f;
            v = m * (1.0f - DEPTH * p) + (1.0f - m) * BLEED * p;
            if (v > 1.0f) v = 1.0f;
            f->chars[y * w + x] = RAMP[(int)(v * RAMP_MAX + 0.5f)];
        }
    }
    return f->chars;
}
