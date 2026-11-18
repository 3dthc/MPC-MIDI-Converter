/* Drives the plugin engine like the MPC would, on a fake drive tree under $ROOT (built with -DMMC_ROOT=...). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "engine.h"
static const mpc_engine_t *E;
static void *I;
static char buf[256];
static const char *get(const char *k) { buf[0] = 0; E->get_param(I, k, buf, sizeof buf); return buf; }
static void tap(const char *k) { E->set_param(I, k, "1"); E->set_param(I, k, "0"); }
static void settle(void) {   /* wait until the worker is idle: status stops changing */
    char last[256] = "x"; for (int i = 0; i < 200; i++) { usleep(20000); if (!strcmp(last, get("display_rev"))) return; strcpy(last, buf); }
}
static int row_of(const char *name) {
    for (int r = 1; r <= 8; r++) { char k[24]; sprintf(k, "file_%d", r); if (strstr(get(k), name)) return r; }
    return 0;
}
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); fails += !ok; }
static void open_row(const char *name) { int r = row_of(name); char k[24]; sprintf(k, "file_%d", r); if (r) tap(k); settle(); }
int main(void) {
    E = mpc_engine();
    I = E->create(NULL); settle();
    check(!strcmp(get("folder"), "Drives"), "starts at the drive list");
    check(row_of("TESTDRIVE") > 0, "drive is listed");
    tap("convert_all"); settle();
    check(strstr(get("status"), "Open a drive") != NULL, "batch at the drive list is refused");
    open_row("TESTDRIVE");
    check(!strcmp(get("folder"), "TESTDRIVE"), "entered the drive");
    open_row("Pack");
    printf("     rows: [%s] ", get("file_1")); printf("[%s] ", get("file_2")); printf("[%s]\n", get("file_3"));
    check(row_of("Sub") == 1, "folders listed first");
    check(row_of("notes.txt") == 0, "non-MIDI files hidden");
    E->set_param(I, "rows", "1");
    check(!strcmp(get("view"), "1"), "list size More switches the view");
    check(row_of("multi_f1") > 0, "8 rows: files on page 1");
    E->set_param(I, "rows", "0");
    check(!strcmp(get("file_8"), " "), "5 rows: row 8 is empty");
    E->set_param(I, "help", "1");
    check(!strcmp(get("view"), "2"), "HOW TO switches the view");
    E->set_param(I, "help", "0");
    tap("convert"); settle();
    check(strstr(get("status"), "Pick a MIDI") != NULL, "convert with nothing picked asks for a file");
    int r = row_of("multi_f1"); char k[24]; sprintf(k, "file_%d", r); tap(k); sprintf(k, "file_%d_on", r);
    check(!strcmp(get(k), "1"), "picked row lights");
    E->set_param(I, "tracks", "1");
    tap("convert"); settle();
    printf("     status: %s\n", get("status"));
    check(strstr(get("status"), "Done: 3 patterns") != NULL, "split file gives 3 patterns");
    E->set_param(I, "tracks", "0");
    tap("convert_all"); settle();
    printf("     status: %s\n", get("status"));
    check(strstr(get("status"), "Done:") != NULL, "batch finishes");
    get("state"); char st[256]; strcpy(st, buf);
    tap("up"); settle();
    check(!strcmp(get("folder"), "TESTDRIVE"), "up goes to the parent");
    tap("up"); settle(); tap("up"); settle();
    check(!strcmp(get("folder"), "Drives"), "up stops at the drive list");
    void *I2 = E->create(NULL); void *keep = I; I = I2; settle();
    E->set_param(I, "state", st); settle();
    check(!strcmp(get("folder"), "TESTDRIVE/Pack"), "state restores the folder");
    E->destroy(I2); I = keep;
    for (int i = 0; i < 2000; i++) { tap("next"); tap("prev"); tap("up"); }   /* tap storm while the worker runs */
    E->destroy(I);
    printf(fails ? "FAILED\n" : "PASSED\n");
    return fails != 0;
}
