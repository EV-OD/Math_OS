#include <user_progs.h>
#include <stdio.h>
#include <process.h>
#include <string.h>
#include <gpu.h>

void prog_counter_a(void) {
    int n = 0;
    for (;;) {
        printf("[A] n=%d\n", n++);
        proc_sleep(50);
    }
}

void prog_counter_b(void) {
    int n = 0;
    for (;;) {
        printf("[B] n=%d\n", n++);
        proc_sleep(75);
    }
}

void prog_spinner(void) {
    const char s[] = "|/-\\";
    int i = 0;
    for (;;) {
        printf("[spin] %c\n", s[i++ % 4]);
        proc_sleep(20);
    }
}

void prog_uhello(void) {
    volatile unsigned long n = 0;
    for (;;) {
        n++;
        if ((n & 0xFFFFF) == 0) proc_sleep(10);
    }
}

void prog_gfx(void) {
    if (gpu_claim() != 0) {
        printf("gfx: GPU screen busy\n");
        proc_exit();
    }
    printf("gfx demo on GPU screen (ctrl+c stops)\n");
    uint32_t w = gpu_width();
    uint32_t h = gpu_height();
    struct ball { int x, y, dx, dy, r; uint32_t c; };
    struct ball b[5] = {
        {100, 100, 3, 2, 28, 0},
        {300, 200, -2, 3, 20, 0},
        {500, 150, 2, -3, 34, 0},
        {200, 400, -3, -2, 24, 0},
        {600, 300, 3, -2, 18, 0},
    };
    b[0].c = gpu_rgb(255, 80, 80);
    b[1].c = gpu_rgb(80, 255, 120);
    b[2].c = gpu_rgb(90, 140, 255);
    b[3].c = gpu_rgb(255, 220, 80);
    b[4].c = gpu_rgb(200, 120, 255);
    uint32_t frame = 0;
    for (;;) {
        for (uint32_t y = 0; y < h; y += 4) {
            uint8_t t = (uint8_t)((y * 90) / h);
            gpu_rect(0, (int)y, (int)w, 4, gpu_rgb(8, 10 + t / 3, 30 + t));
        }
        for (int i = 0; i < 5; i++) {
            b[i].x += b[i].dx;
            b[i].y += b[i].dy;
            if (b[i].x - b[i].r < 0 || b[i].x + b[i].r >= (int)w) b[i].dx = -b[i].dx;
            if (b[i].y - b[i].r < 0 || b[i].y + b[i].r >= (int)h) b[i].dy = -b[i].dy;
            gpu_circle_fill(b[i].x, b[i].y, b[i].r, b[i].c);
            gpu_circle(b[i].x, b[i].y, b[i].r, gpu_rgb(255, 255, 255));
        }
        gpu_rect_outline(0, 0, (int)w, (int)h, gpu_rgb(255, 255, 255));
        gpu_text(12, 12, "GPU DEMO - bouncing balls", gpu_rgb(255, 255, 255));
        char f[32];
        sprintf(f, "frame %d", frame);
        gpu_text(12, 48, f, gpu_rgb(255, 255, 0));
        gpu_present();
        frame++;
        proc_sleep(3);
    }
}

const user_prog_t user_progs[] = {
    {"a", prog_counter_a, 0, "kernel counter A"},
    {"b", prog_counter_b, 0, "kernel counter B"},
    {"spin", prog_spinner, 0, "spinner demo"},
    {"uhello", prog_uhello, 1, "ring3 silent worker"},
    {"gfx", prog_gfx, 0, "GPU animation demo"},
    {"glcube", prog_glcube, 0, "OpenGL rotating cube"},
};

const int user_progs_count = sizeof(user_progs) / sizeof(user_progs[0]);

const user_prog_t *user_find(const char *name) {
    for (int i = 0; i < user_progs_count; i++) {
        if (strcmp(user_progs[i].name, name) == 0) return &user_progs[i];
    }
    return 0;
}
