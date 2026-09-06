#include <ui.h>
#include <display.h>
#include <stdio.h>
#include <string.h>
#include <process.h>
#include <gpu.h>

#define BAR_H 40

static void busy_wait(void) {
    for (volatile unsigned i = 0; i < 8000000u; i++) {}
}

void ui_splash(void) {
    uint32_t W = display_width(), H = display_height();
    display_set_bg(0x000000);
    display_set_fg(UI_FG);
    display_set_region(0, 0, W, H);
    display_clear();
    set_cursor((W - 4 * 18) / 2, H / 2 - 60);
    draw_string("MyOS");
    display_set_fg(UI_ACCENT);
    set_cursor((W - 23 * 18) / 2, H / 2);
    draw_string("a tiny multitasking os");
    display_set_fg(UI_FG);
    draw_rect(W / 2 - 200, H / 2 + 80, 400, 20, UI_STATBG);
    for (int i = 0; i <= 10; i++) {
        draw_rect(W / 2 - 200, H / 2 + 80, i * 40, 20, UI_ACCENT);
        busy_wait();
    }
    display_set_bg(UI_BG);
    display_set_fg(UI_FG);
}

void ui_chrome(void) {
    uint32_t W = display_width(), H = display_height();
    display_set_bg(UI_BG);
    display_set_fg(UI_FG);
    draw_rect(0, 0, W, BAR_H, UI_TITLEBG);
    draw_rect(0, BAR_H - 2, W, 2, UI_ACCENT);
    set_cursor(12, 4);
    draw_string("MyOS // shell | canvas");
    set_cursor(W - 6 * 18, 4);
    display_set_fg(UI_DIM);
    draw_string("v0.1");
    display_set_fg(UI_FG);
    if (W >= 1000 && H >= 600) {
        uint32_t mid = W / 2;
        uint32_t bot = BAR_H + 18 * 34;
        if (bot > H - BAR_H) bot = H - BAR_H;
        display_set_region(0, BAR_H, mid, bot);
        display_clear();
        draw_rect(0, bot, W, H - BAR_H - bot, UI_BG);
        draw_rect(mid - 2, BAR_H, 2, H - BAR_H - BAR_H, UI_ACCENT);
        gpu_set_view((int)mid, (int)BAR_H, (int)(W - mid), (int)(H - BAR_H - BAR_H));
        gpu_clear(0x040610);
        gpu_present();
        display_set_fg(UI_DIM);
        display_text_at(mid + 12, BAR_H + 4, "CANVAS");
        display_set_fg(UI_FG);
    } else {
        display_set_region(0, BAR_H, W, H - BAR_H);
        display_clear();
        gpu_set_view((int)W - 1, (int)H - 1, 1, 1);
    }
    draw_rect(0, H - BAR_H, W, BAR_H, UI_STATBG);
    draw_rect(0, H - BAR_H, W, 2, UI_ACCENT);
    ui_status_tick();
}

void ui_status_tick(void) {
    uint32_t W = display_width(), H = display_height();
    uint32_t y = H - BAR_H + 4;
    sched_lock();
    draw_rect(0, y, W, 32, UI_STATBG);
    display_set_fg(UI_ACCENT);
    display_text_at(12, y, "MyOS");
    display_set_fg(UI_DIM);
    char b[64];
    sprintf(b, "  ticks %d  procs %d", timer_now(), proc_count());
    display_text_at(12 + 4 * 18, y, b);
    const char *hint = "pgup/pgdn scroll";
    display_text_at(W - (uint32_t)strlen(hint) * 18 - 12, y, (char *)hint);
    display_set_fg(UI_FG);
    sched_unlock();
}

void statusd(void) {
    for (;;) {
        ui_status_tick();
        proc_sleep(100);
    }
}
