/* evread: print the input events a device emits, for a few seconds, then exit. Read-only.
 * Usage: ./evread /dev/input/eventN [seconds]   (default 6). Decodes input_event, skips SYN.
 * Uses select() so it needs no external 'timeout'. evdev allows many readers, so run it alongside MPC. */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static const char *tname(int t) {
    switch (t) { case EV_KEY: return "KEY"; case EV_REL: return "REL"; case EV_ABS: return "ABS";
                 case EV_MSC: return "MSC"; default: return "?"; }
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: evread /dev/input/eventN [seconds]\n"); return 2; }
    int secs = argc > 2 ? atoi(argv[2]) : 6;
    int fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if (fd < 0) { perror("open"); return 1; }
    fprintf(stderr, "reading %s for %ds - operate the control now\n", argv[1], secs);
    time_t end = time(NULL) + secs;
    int seen = 0;
    while (time(NULL) < end) {
        fd_set r; FD_ZERO(&r); FD_SET(fd, &r);
        struct timeval tv = {1, 0};
        if (select(fd + 1, &r, NULL, NULL, &tv) <= 0) continue;
        struct input_event e;
        while (read(fd, &e, sizeof e) == (ssize_t)sizeof e)
            if (e.type != EV_SYN) { printf("%-3s code=%-4d value=%d\n", tname(e.type), e.code, e.value); seen++; }
    }
    fprintf(stderr, seen ? "-- done (%d events)\n" : "-- done (no events on this device)\n", seen);
    return 0;
}
