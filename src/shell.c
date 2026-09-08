#include <shell.h>
#include <cmd.h>
#include <stdio.h>
#include <display.h>
#include <ui.h>

void shell_exec(char *line) {
    cmd_ctx_t ctx;
    ctx.pane_id = -1;
    ctx.canvas_id = 0;
    ctx.has_canvas = 0;
    ctx.strict = 0;
    if (cmd_dispatch(&ctx, line)) return;
    line = (char *)cmd_skip_spaces(line);
    printf("unknown: %s (try help)\n", line);
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
