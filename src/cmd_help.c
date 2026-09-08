#include <cmd.h>
#include <user_progs.h>
#include <stdio.h>

static int fn_help(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    printf("cmds: help keys ps run <prog|file.ez> kill <pid> echo <t> uptime clear\n");
    printf("fs: ls lt cat mkdir cd code (ram only, gone on reboot)\n");
    printf("math: calc <expr> | plot <expr> [a] [b] | fft [expr] [n] | freq | N> autofit\n");
    printf("dsp: dft <expr> [n] | adc <expr> <bits> [n] | dac | fft adc\n");
    printf("flt: fir avg N | fir sinc fc [taps] | iir lp|hp fc | freqz | pz | filter <expr>\n");
    printf("ana: afft [expr] [Fs] [n] (Hz spectrum)\n");
    printf("funcs: h(x)=sin(x) | h(2) | g(x)=h(x)/x | N# sticky canvas\n");
    printf("view: z+ z- z<< N z>> N (prefix with N> for canvas N)\n");
    printf("progs:");
    for (int i = 0; i < user_progs_count; i++)
        printf(" %s", user_progs[i].name);
    printf("\n");
    return 0;
}

static int fn_keys(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    printf("prefix Ctrl+b then:\n");
    printf(" \" horiz split | %%/v vert split | x kill pane\n");
    printf(" h/j/k/l or arrows: focus | H/J/K/L or Ctrl+arrows: resize\n");
    printf(" c new window | n/p next/prev window | 0-9 select window\n");
    printf("editing: Left/Right move, Del delete, Backspace erase\n");
    printf("canvas: canvas | N> plot/fft/grid/autofit | N# sticky | z+/z-/z<</z>> view\n");
    return 0;
}

const cmd_entry_t cmd_g_help[] = {
    {"help", fn_help, "help", "list commands", 0, 0, 0, 0, 0, 0},
    {"keys", fn_keys, "keys", "list key shortcuts", 0, 0, 0, 0, 0, 0},
};
const int cmd_g_help_n = 2;
