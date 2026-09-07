#include <shell.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <user_progs.h>
#include <display.h>
#include <mat.h>
#include <ui.h>
#include <gpu.h>

static const char *skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static int has_equals(const char *s) {
    while (*s) {
        if (*s == '=') return 1;
        s++;
    }
    return 0;
}

void shell_exec(char *line) {
    line = (char *)skip_spaces(line);
    if (!*line) return;

    if (strcmp(line, "help") == 0) {
        printf("cmds: help ps run <name> kill <pid> echo <t> uptime clear ctrl+c pgup/pgdn\n");
        printf("math: calc <expr> | plot <expr> <a> <b> | fft [n] | N> autofit\n");
        printf("progs:");
        for (int i = 0; i < user_progs_count; i++)
            printf(" %s", user_progs[i].name);
        printf("\n");
    } else if (strcmp(line, "ps") == 0) {
        ps_list();
    } else if (strncmp(line, "run ", 4) == 0) {
        const char *name = skip_spaces(line + 4);
        const user_prog_t *p = user_find(name);
        if (!p) { printf("unknown prog: %s\n", name); return; }
        int pid = process_create(p->entry, p->name, p->user);
        if (pid < 0) printf("no free slot\n");
        else printf("started pid=%d (%s)\n", pid, p->name);
    } else if (strncmp(line, "kill ", 5) == 0) {
        int pid = atoi(skip_spaces(line + 5));
        if (proc_kill(pid) == 0) printf("killed %d\n", pid);
        else printf("kill failed\n");
    } else if (strncmp(line, "echo ", 5) == 0) {
        printf("%s\n", line + 5);
    } else if (strcmp(line, "uptime") == 0) {
        printf("ticks=%d\n", timer_now());
    } else if (strcmp(line, "clear") == 0) {
        display_clear();
    } else if (strcmp(line, "clear canvas") == 0) {
        if (gpu_owned()) printf("canvas busy\n");
        else { gpu_clear(0x040610); gpu_present(); }
    } else if (strcmp(line, "calc") == 0 || strncmp(line, "calc ", 5) == 0) {
        cmd_calc(line[4] ? line + 5 : "");
    } else if (strncmp(line, "plot ", 5) == 0) {
        cmd_plot(line + 5);
    } else if (strcmp(line, "fft") == 0 || strncmp(line, "fft ", 4) == 0) {
        cmd_fft(line[3] ? line + 4 : "");
    } else if (has_equals(line)) {
        cmd_calc(line);
    } else {
        printf("unknown: %s (try help)\n", line);
    }
}

void shell_run(void) {
    printf("shell on serial + screen. try 'help', 'run glcube', 'ps'\n");
    for (;;) {
        display_set_fg(UI_ACCENT);
        char *line = input("shell> ");
        display_set_fg(UI_FG);
        shell_exec(line);
    }
}
