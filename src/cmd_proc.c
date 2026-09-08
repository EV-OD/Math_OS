#include <cmd.h>
#include <process.h>
#include <user_progs.h>
#include <tmux.h>
#include <stdio.h>
#include <string.h>

static int fn_ps(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    ps_list();
    return 0;
}

static int fn_run(cmd_ctx_t *ctx, const char *args) {
    const char *name = cmd_skip_spaces(args);
    if (!*name) {
        printf("unknown: run (try help)\n");
        return 0;
    }
    const user_prog_t *p = user_find(name);
    if (p) {
        int pid = process_create(p->entry, p->name, p->user);
        if (pid < 0) printf("no free slot\n");
        else printf("started pid=%d (%s)\n", pid, p->name);
        return 0;
    }
    if (ctx->pane_id >= 0) {
        char prog[64];
        int ni = 0;
        while (name[ni] && name[ni] != ' ' && name[ni] != '\t' && ni < 63) {
            prog[ni] = name[ni];
            ni++;
        }
        prog[ni] = 0;
        if (*name && tmux_try_run_file(ctx->pane_id, prog) == 0) return 0;
    }
    printf("unknown prog: %s\n", name);
    return 0;
}

static int fn_kill(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    args = cmd_skip_spaces(args);
    if (!*args) {
        printf("unknown: kill (try help)\n");
        return 0;
    }
    int pid = atoi(cmd_skip_spaces(args));
    if (proc_kill(pid) == 0) printf("killed %d\n", pid);
    else printf("kill failed\n");
    return 0;
}

const cmd_entry_t cmd_g_proc[] = {
    {"ps", fn_ps, "ps", "list processes", 0, 0, 0, 0, 0, 0},
    {"run", fn_run, "run <prog|file.ez>", "start program or script", 1, 0, 0, 0, 0, 0},
    {"kill", fn_kill, "kill <pid>", "stop process", 0, 0, 0, 0, 0, 0},
};
const int cmd_g_proc_n = 3;
