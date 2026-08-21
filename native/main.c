#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdalign.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "field.h"

#define W 80
#define H 24
#define FRAME_NS 33333333L /* 30 fps */

static alignas(4) unsigned char mem[FIELD_BYTES(W, H)];

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

    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && (ws.ws_col < W || ws.ws_row < H)) {
        fprintf(stderr, "terminal too small: need %dx%d\n", W, H);
        return 1;
    }
    field_init(&f, W, H, (uint32_t)time(NULL), 0.94f, mem);

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
        for (int y = 0; y < H; y++) {
            fwrite(chars + y * W, 1, W, stdout);
            if (y < H - 1)
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
