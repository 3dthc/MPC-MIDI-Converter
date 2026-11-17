/* Test CLI: cli <in.mid> <outdir> [split drum quant transpose trim bpm] */
#include <stdio.h>
#include <stdlib.h>
#include "../src/convert.h"
int main(int c, char **v) {
    conv_opts_t o = {0};
    if (c > 3) o.split = atoi(v[3]);
    if (c > 4) o.drum_map = atoi(v[4]);
    if (c > 5) o.quantize = atoi(v[5]);
    if (c > 6) o.transpose = atoi(v[6]);
    if (c > 7) o.trim = atoi(v[7]);
    if (c > 8) o.bpm_in_name = atoi(v[8]);
    char e[128] = "";
    int r = mmc_convert_file(v[1], v[2], &o, e, sizeof e);
    printf("%d %s\n", r, e);
    return r < 0;
}
