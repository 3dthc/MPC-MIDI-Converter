/* touchcap: dump raw touchscreen events, to find the tool-icon coordinates (or prove the screen is grabbed).
 * Usage on the MPC:  chmod +x tc; ./tc   (optionally: ./tc /dev/input/event0)   Ctrl+C to stop. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    const char *dev = argc > 1 ? argv[1] : "/dev/input/event0";
    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    fprintf(stderr, "Reading %s - tap the screen once. If nothing prints, the screen is grabbed. Ctrl+C to stop.\n", dev);
    int x = -1, y = -1;
    struct input_event e;
    while (read(fd, &e, sizeof e) == (ssize_t)sizeof e) {
        if (e.type == EV_SYN) continue;                 /* skip sync noise */
        if (e.type == EV_ABS) {
            if (e.code == ABS_MT_POSITION_X || e.code == ABS_X) x = e.value;
            else if (e.code == ABS_MT_POSITION_Y || e.code == ABS_Y) y = e.value;
        }
        printf("type=%d code=%d val=%d   x=%d y=%d\n", e.type, e.code, e.value, x, y);
        fflush(stdout);
    }
    return 0;
}
