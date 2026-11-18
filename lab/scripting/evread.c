/* evread: print the input events a device emits. Read-only. Usage on the MPC: ./evread /dev/input/event1
 * Decodes Linux input_event (type/code/value), skipping SYN. Open alongside MPC (evdev allows many readers). */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char *tname(int t) {
    switch (t) { case EV_KEY: return "KEY"; case EV_REL: return "REL"; case EV_ABS: return "ABS";
                 case EV_MSC: return "MSC"; default: return "?"; }
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: evread /dev/input/eventN\n"); return 2; }
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct input_event e;
    fprintf(stderr, "reading %s (Ctrl-C to stop) - operate the control now\n", argv[1]);
    while (read(fd, &e, sizeof e) == (ssize_t)sizeof e)
        if (e.type != EV_SYN) printf("%-3s code=%-4d value=%d\n", tname(e.type), e.code, e.value);
    return 0;
}
