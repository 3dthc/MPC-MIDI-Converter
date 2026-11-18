/* midicap: read-only capture of MPC's control-surface MIDI (the panel: buttons, data wheel, Q-Links).
 * MPC reads this as ALSA rawmidi on a "Private" port. We hook snd_rawmidi_open to spot that handle and
 * snd_rawmidi_read to log the bytes MPC receives. Logs /tmp/mpc-midicap.txt. Injects/changes nothing.
 * Hooking approach follows hwremap by mmiroshnikov (MIT, open-mpc/mpc-vst-plugins tools/mpc_patch/hwremap). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct _snd_rawmidi snd_rawmidi_t;
typedef struct _snd_rawmidi_info snd_rawmidi_info_t;

static int (*real_open)(snd_rawmidi_t **, snd_rawmidi_t **, const char *, int);
static ssize_t (*real_read)(snd_rawmidi_t *, void *, size_t);
static snd_rawmidi_t *priv_in;

/* libc's own dlsym, so RTLD_NEXT is resolved relative to this library even behind other shims (hwremap note) */
static void *next_sym(const char *name) {
    static void *(*libc_dlsym)(void *, const char *);
    if (!libc_dlsym) {
        static const char *const vers[] = {"2.34", "2.17", "2.4", "2.2.5", "2.0"};
        for (size_t i = 0; i < sizeof vers / sizeof *vers && !libc_dlsym; i++)
            libc_dlsym = (void *(*)(void *, const char *))dlvsym(RTLD_DEFAULT, "dlsym", vers[i]);
    }
    return (libc_dlsym ? libc_dlsym : dlsym)(RTLD_NEXT, name);
}
static void init_syms(void) {
    if (!real_open) real_open = next_sym("snd_rawmidi_open");
    if (!real_read) real_read = next_sym("snd_rawmidi_read");
}

static void logline(const char *s) {
    static int (*ro)(const char *, int, ...);
    if (!ro) ro = (int (*)(const char *, int, ...))next_sym("open");
    int fd = ro("/tmp/mpc-midicap.txt", O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd >= 0) { if (write(fd, s, strlen(s)) < 0) {} close(fd); }
}

static int is_private(snd_rawmidi_t *h, const char *name) {
    size_t (*info_sizeof)(void) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info_sizeof");
    int (*info)(snd_rawmidi_t *, snd_rawmidi_info_t *) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info");
    const char *(*subname)(const snd_rawmidi_info_t *) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info_get_subdevice_name");
    const char *(*devname)(const snd_rawmidi_info_t *) = dlsym(RTLD_DEFAULT, "snd_rawmidi_info_get_name");
    int priv = 0;
    if (info_sizeof && info) {
        snd_rawmidi_info_t *ri = calloc(1, info_sizeof());
        if (ri && info(h, ri) == 0) {
            const char *sn = subname ? subname(ri) : NULL, *dn = devname ? devname(ri) : NULL;
            char msg[256];
            snprintf(msg, sizeof msg, "open port: name=%s sub=%s\n", dn ? dn : "?", sn ? sn : "?");
            logline(msg);
            if ((sn && strstr(sn, "Private")) || (dn && strstr(dn, "Private"))) priv = 1;
        }
        free(ri);
    }
    if (name) { char m[128]; snprintf(m, sizeof m, "open name arg: %s\n", name); logline(m); }
    return priv;
}

__attribute__((visibility("default")))
int snd_rawmidi_open(snd_rawmidi_t **in, snd_rawmidi_t **out, const char *name, int mode) {
    init_syms();
    int r = real_open(in, out, name, mode);
    if (r == 0 && in && *in && is_private(*in, name)) { priv_in = *in; logline("-> tagged as Private input\n"); }
    return r;
}

__attribute__((visibility("default")))
ssize_t snd_rawmidi_read(snd_rawmidi_t *h, void *buf, size_t size) {
    init_syms();
    ssize_t r = real_read(h, buf, size);
    if (h == priv_in && r > 0) {
        const unsigned char *b = buf;
        char line[8 + 3 * 64 + 1];
        int k = snprintf(line, sizeof line, "in:");
        for (ssize_t i = 0; i < r && i < 64 && k < (int)sizeof line - 4; i++) k += snprintf(line + k, sizeof line - k, " %02X", b[i]);
        if (k < (int)sizeof line - 1) line[k++] = '\n', line[k] = 0;
        logline(line);
    }
    return r;
}

__attribute__((constructor)) static void hello(void) { logline("== midicap loaded\n"); }
