#include <cmd.h>
#include <process.h>
#include <display.h>
#include <gpu.h>
#include <stdio.h>
#include <string.h>

static int fn_echo(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    if (!*args) {
        printf("unknown: echo (try help)\n");
        return 0;
    }
    printf("%s\n", args);
    return 0;
}

static int fn_uptime(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    printf("ticks=%d\n", timer_now());
    return 0;
}

static int fn_clear(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    if (!strcmp(args, "canvas")) {
        if (gpu_owned()) printf("canvas busy\n");
        else {
            gpu_clear(0x040610);
            gpu_present();
        }
        return 0;
    }
    if (*args) {
        printf("unknown: clear %s (try help)\n", args);
        return 0;
    }
    display_clear();
    return 0;
}

const cmd_entry_t cmd_g_sys[] = {
    {"echo", fn_echo, "echo <text>", "print text", 0, 0, 0, 0, 0, 0},
    {"uptime", fn_uptime, "uptime", "show ticks", 0, 0, 0, 0, 0, 0},
    {"clear", fn_clear, "clear [canvas]", "clear screen", 0, 0, 0, 0, 0, 0},
};
const int cmd_g_sys_n = 3;
