#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "field.h"

#define MAXW 320
#define MAXH 96
#define FRAME_NS 33333333L /* 30 fps */

static alignas(4) unsigned char mem[FIELD_BYTES(MAXW, MAXH)];

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
    field_init(&f, w, h, (uint32_t)time(NULL), 0.94f, mem);

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
