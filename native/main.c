#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "field.h"

#define MAXW 320
#define MAXH 96
#define FRAME_NS 33333333L /* 30 fps */
#define GW 5               /* glyph columns, pitch is GW + 1 */
#define GH 7               /* glyph rows, pitch is GH + 1 */

static alignas(4) unsigned char mem[FIELD_BYTES(MAXW, MAXH)];

/* 5x7 glyphs, one byte per row, bit 4 is the left column */
static const struct { char c; uint8_t row[GH]; } FONT[] = {
    { 'J', { 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c } },
    { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'M', { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    { 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } },
    { 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } },
    { 'C', { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } },
    { 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
};

static const uint8_t *glyph(char c)
{
    for (size_t i = 0; i < sizeof FONT / sizeof FONT[0]; i++)
        if (FONT[i].c == c)
            return FONT[i].row;
    return NULL;
}

/* stamp word on row y0, sx by sy cells per glyph pixel, centred */
static void stamp(uint8_t *mask, int w, int h, const char *word, int y0, int sx, int sy)
{
    int len = (int)strlen(word);
    int x0 = (w - (len * (GW + 1) - 1) * sx) / 2;

    for (int g = 0; g < len; g++) {
        const uint8_t *rows = glyph(word[g]);
        if (!rows)
            continue;
        for (int r = 0; r < GH; r++)
            for (int c = 0; c < GW; c++) {
                if (!(rows[r] & (0x10 >> c)))
                    continue;
                for (int dy = 0; dy < sy; dy++)
                    for (int dx = 0; dx < sx; dx++) {
                        int x = x0 + (g * (GW + 1) + c) * sx + dx;
                        int y = y0 + r * sy + dy;
                        if (x >= 0 && x < w && y >= 0 && y < h)
                            mask[y * w + x] = 255;
                    }
            }
    }
}

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static void on_int(int sig)
{
    (void)sig;
    ssize_t r = write(1, "\x1b[?25h\n", 7);
    (void)r;
    _exit(0);
}

int main(void)
{
    Field f;
    struct winsize ws;
    int w = 80, h = 24;

    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col && ws.ws_row) {
        w = ws.ws_col < MAXW ? ws.ws_col : MAXW;
        h = ws.ws_row < MAXH ? ws.ws_row : MAXH;
    }
    field_init(&f, w, h, (uint32_t)time(NULL), 0.5f, mem);

    /* two lines; terminal cells are twice as tall as wide, so a glyph
       pixel is up to twice as many cells wide as tall */
    int sy = h / (2 * (GH + 1)), sx = w / (6 * (GW + 1) - 1);
    if (sy < 1)
        sy = 1;
    if (sx > 2 * sy)
        sx = 2 * sy;
    if (sx < 1)
        sx = 1;
    uint8_t *mask = field_mask(&f);
    int top = (h - (2 * GH + 1) * sy) / 2;
    stamp(mask, w, h, "JAMES", top, sx, sy);
    stamp(mask, w, h, "CLARKE", top + (GH + 1) * sy, sx, sy);
    field_seed(&f);

    signal(SIGINT, on_int);
    signal(SIGTERM, on_int);
    signal(SIGHUP, on_int);
    signal(SIGQUIT, on_int);
    fputs("\x1b[2J\x1b[?25l", stdout);

    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    double prev = now_s();

    for (;;) {
        double t = now_s();
        float dt = (float)(t - prev);
        prev = t;
        if (dt > 0.1f)
            dt = 0.1f;

        field_step(&f, dt);
        const char *chars = field_chars(&f);

        fputs("\x1b[H", stdout);
        for (int y = 0; y < h; y++) {
            fwrite(chars + y * w, 1, w, stdout);
            if (y < h - 1)
                putchar('\n');
        }
        fflush(stdout);

        deadline.tv_nsec += FRAME_NS;
        while (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_nsec -= 1000000000L;
            deadline.tv_sec++;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
    }
}
