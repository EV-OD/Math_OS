#include <cmd.h>
#include <fs.h>
#include <tmux.h>
#include <stdio.h>
#include <string.h>

static int fn_list(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    const char *p = cmd_skip_spaces(args);
    if (!*p) p = ".";
    char out[1024];
    if (fs_list(p, out, sizeof(out)) < 0) printf("ls: no such dir\n");
    else printf("%s", out);
    return 0;
}

static int fn_cat(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    const char *p = cmd_skip_spaces(args);
    if (!*p) {
        printf("unknown: cat (try help)\n");
        return 0;
    }
    char out[2048];
    if (fs_read(p, out, sizeof(out)) < 0) printf("cat: no such file\n");
    else printf("%s", out);
    return 0;
}

static int fn_mkdir(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    const char *p = cmd_skip_spaces(args);
    if (!*p) {
        printf("unknown: mkdir (try help)\n");
        return 0;
    }
    if (fs_mkdir(p) < 0) printf("mkdir failed\n");
    return 0;
}

static int fn_cd(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    const char *p = cmd_skip_spaces(args);
    if (!*p) p = "/";
    if (fs_cd(p) < 0) printf("cd: no such dir\n");
    else printf("%s\n", fs_cwd());
    return 0;
}

static int fn_code(cmd_ctx_t *ctx, const char *args) {
    const char *nm = cmd_skip_spaces(args);
    if (!*nm) {
        printf("usage: code <file>\n");
        return 0;
    }
    char path[96];
    int pi = 0;
    while (nm[pi] && nm[pi] != ' ' && nm[pi] != '\t' && pi < 95) {
        path[pi] = nm[pi];
        pi++;
    }
    path[pi] = 0;
    tmux_edit_file(ctx->pane_id, path);
    return 0;
}

const cmd_entry_t cmd_g_fs[] = {
    {"ls", fn_list, "ls [dir]", "list directory", 0, 0, 0, 0, 0, 0},
    {"lt", fn_list, "lt [dir]", "list directory", 0, 0, 0, 0, 0, 0},
    {"cat", fn_cat, "cat <file>", "show file", 0, 0, 0, 0, 0, 0},
    {"mkdir", fn_mkdir, "mkdir <dir>", "make directory", 0, 0, 0, 0, 0, 0},
    {"cd", fn_cd, "cd [dir]", "change directory", 0, 0, 0, 0, 0, 0},
    {"code", fn_code, "code <file>", "edit file", 1, 1, 0, 0, 0, 0},
};
const int cmd_g_fs_n = 6;
