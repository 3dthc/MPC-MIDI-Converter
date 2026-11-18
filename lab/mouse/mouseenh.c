/* mouseenh: a mouse enhancer for MPC standalone. Grabs the real USB mouse and presents a virtual mouse through
 * /dev/uinput. Movement and buttons pass straight through, so normal use is unchanged. The scroll WHEEL, which MPC
 * ignores, is turned into a hold-drag (press + vertical drag + release) - a gesture MPC's touch UI already obeys -
 * so rolling the wheel scrolls. No MPC modification: MPC only ever receives ordinary pointer input.
 *
 * Usage: mouseenh /dev/input/eventN [step] [invert]   step = px per notch (default 40), invert = 1 to flip direction.
 * Start it BEFORE MPC opens mice (the runner restarts MPC after starting this), so MPC binds the virtual device. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static int ufd;
static void emit(int type, int code, int val) {
    struct input_event e; memset(&e, 0, sizeof e);
    e.type = type; e.code = code; e.value = val;
    if (write(ufd, &e, sizeof e) < 0) {}
}
static void syn(void) { emit(EV_SYN, SYN_REPORT, 0); }

static long long now_ms(void) { struct timeval t; gettimeofday(&t, NULL); return (long long)t.tv_sec * 1000 + t.tv_usec / 1000; }

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: mouseenh /dev/input/eventN [step] [invert]\n"); return 2; }
    int step = argc > 2 ? atoi(argv[2]) : 40;
    int invert = argc > 3 ? atoi(argv[3]) : 0;
    const int RELEASE_MS = 160;   /* release the synthetic hold this long after the last wheel notch */

    int mfd = open(argv[1], O_RDONLY);
    if (mfd < 0) { perror("open mouse"); return 1; }
    if (ioctl(mfd, EVIOCGRAB, 1) < 0) { perror("grab"); return 1; }   /* take the real mouse so MPC sees only ours */

    ufd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (ufd < 0) { perror("open uinput"); return 1; }
    ioctl(ufd, UI_SET_EVBIT, EV_KEY); ioctl(ufd, UI_SET_EVBIT, EV_REL); ioctl(ufd, UI_SET_EVBIT, EV_SYN);
    ioctl(ufd, UI_SET_KEYBIT, BTN_LEFT); ioctl(ufd, UI_SET_KEYBIT, BTN_RIGHT); ioctl(ufd, UI_SET_KEYBIT, BTN_MIDDLE);
    ioctl(ufd, UI_SET_RELBIT, REL_X); ioctl(ufd, UI_SET_RELBIT, REL_Y); ioctl(ufd, UI_SET_RELBIT, REL_WHEEL);
    struct uinput_user_dev ud; memset(&ud, 0, sizeof ud);
    snprintf(ud.name, sizeof ud.name, "mouseenh virtual mouse");
    ud.id.bustype = BUS_USB; ud.id.vendor = 0x3d7; ud.id.product = 0x1; ud.id.version = 1;
    if (write(ufd, &ud, sizeof ud) < 0) { perror("uinput dev"); return 1; }
    if (ioctl(ufd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); return 1; }
    fprintf(stderr, "mouseenh running on %s (step %d, invert %d)\n", argv[1], step, invert);

    int scrolling = 0, user_left = 0;
    long long last_wheel = 0;
    for (;;) {
        struct pollfd p = { mfd, POLLIN, 0 };
        int pr = poll(&p, 1, 20);
        long long t = now_ms();
        if (scrolling && t - last_wheel > RELEASE_MS) { emit(EV_KEY, BTN_LEFT, 0); syn(); scrolling = 0; }
        if (pr <= 0) continue;
        struct input_event e;
        ssize_t n = read(mfd, &e, sizeof e);
        if (n != (ssize_t)sizeof e) { if (errno == EINTR) continue; break; }

        if (e.type == EV_REL && (e.code == REL_WHEEL || e.code == REL_WHEEL_HI_RES)) {
            if (e.code == REL_WHEEL_HI_RES) continue;            /* use the notch event, ignore the hi-res twin */
            if (user_left) { emit(EV_REL, REL_WHEEL, e.value); syn(); continue; }  /* don't fight a real drag */
            int dir = (e.value > 0 ? 1 : -1) * (invert ? -1 : 1);
            if (!scrolling) { emit(EV_KEY, BTN_LEFT, 1); scrolling = 1; }
            emit(EV_REL, REL_Y, -dir * step);                    /* wheel up -> content up (finger moves up) */
            syn();
            last_wheel = t;
            continue;
        }
        if (e.type == EV_KEY && (e.code == BTN_LEFT || e.code == BTN_RIGHT || e.code == BTN_MIDDLE)) {
            if (scrolling) { emit(EV_KEY, BTN_LEFT, 0); syn(); scrolling = 0; }  /* a real click ends a scroll */
            if (e.code == BTN_LEFT) user_left = e.value;
            emit(EV_KEY, e.code, e.value); syn(); continue;
        }
        if (e.type == EV_REL && (e.code == REL_X || e.code == REL_Y)) { emit(EV_REL, e.code, e.value); syn(); continue; }
        if (e.type == EV_SYN) continue;   /* we SYN after each of our own emits */
    }
    ioctl(ufd, UI_DEV_DESTROY); close(ufd); ioctl(mfd, EVIOCGRAB, 0); close(mfd);
    return 0;
}
