#include <gpu.h>
#include <display.h>
#include <process.h>
#include <font.h>
#include <string.h>
#include <log.h>

#define GPU_MAX_W 1920
#define GPU_MAX_H 1080

static uint32_t backbuf[GPU_MAX_W * GPU_MAX_H];
static uint32_t fb_addr;
static uint32_t fb_pitch;
static uint32_t fb_w;
static uint32_t fb_h;
static int use_direct;
static int vx, vy, vw, vh;
static volatile int owner = -1;

void gpu_init(uint32_t addr, uint32_t pitch, uint32_t w, uint32_t h) {
    fb_addr = addr;
    fb_pitch = pitch;
    fb_w = w;
    fb_h = h;
    use_direct = (w > GPU_MAX_W || h > GPU_MAX_H) ? 1 : 0;
    vx = 0;
    vy = 0;
    vw = (int)w;
    vh = (int)h;
    if (!use_direct) memset(backbuf, 0, sizeof(backbuf));
}

void gpu_set_view(int x, int y, int w, int h) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if ((uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if ((uint32_t)(x + w) > fb_w) w = (int)fb_w - x;
    if ((uint32_t)(y + h) > fb_h) h = (int)fb_h - y;
    vx = x;
    vy = y;
    vw = w;
    vh = h;
    log_debug("ui: gpu_view %d,%d %dx%d", x, y, w, h);
}

uint32_t gpu_width(void) { return (uint32_t)vw; }
uint32_t gpu_height(void) { return (uint32_t)vh; }
uint32_t gpu_screen_width(void) { return fb_w; }
uint32_t gpu_screen_height(void) { return fb_h; }
int gpu_vx(void) { return vx; }
int gpu_vy(void) { return vy; }

uint32_t gpu_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

int gpu_claim(void) {
    if (owner >= 0) { log_debug("ui: gpu_claim busy owner=%d", owner); return -1; }
    owner = proc_self();
    log_debug("ui: gpu_claim owner=%d", owner);
    gpu_clear(0x040610);
    gpu_present();
    return 0;
}

void gpu_release(void) {
    log_debug("ui: gpu_release owner=%d", owner);
    owner = -1;
    gpu_clear(0x040610);
    gpu_present();
}

void gpu_release_if_owner(int pid) {
    if (owner == pid) gpu_release();
}

int gpu_owned(void) { return owner >= 0; }

static uint32_t *px_addr(int x, int y) {
    if (x < 0 || y < 0 || x >= vw || y >= vh) return 0;
    uint32_t ax = (uint32_t)(vx + x);
    uint32_t ay = (uint32_t)(vy + y);
    if (use_direct) {
        if (ax >= fb_w || ay >= fb_h) return 0;
        return (uint32_t *)((uint8_t *)(uintptr_t)fb_addr + ay * fb_pitch + ax * 4);
    }
    return &backbuf[ay * fb_w + ax];
}

void gpu_clear(uint32_t color) {
    for (int y = 0; y < vh; y++)
        for (int x = 0; x < vw; x++) {
            uint32_t *p = px_addr(x, y);
            if (p) *p = color;
        }
}

void gpu_pixel(int x, int y, uint32_t color) {
    uint32_t *p = px_addr(x, y);
    if (p) *p = color;
}

void gpu_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            gpu_pixel(i, j, color);
}

void gpu_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) { gpu_pixel(i, y, color); gpu_pixel(i, y + h - 1, color); }
    for (int j = y; j < y + h; j++) { gpu_pixel(x, j, color); gpu_pixel(x + w - 1, j, color); }
}

void gpu_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        gpu_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void gpu_circle(int cx, int cy, int r, uint32_t color) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        gpu_pixel(cx + x, cy + y, color); gpu_pixel(cx + y, cy + x, color);
        gpu_pixel(cx - y, cy + x, color); gpu_pixel(cx - x, cy + y, color);
        gpu_pixel(cx - x, cy - y, color); gpu_pixel(cx - y, cy - x, color);
        gpu_pixel(cx + y, cy - x, color); gpu_pixel(cx + x, cy - y, color);
        y++;
        if (err <= 0) err += 2 * y + 1;
        else { x--; err -= 2 * x + 1; }
    }
}

void gpu_circle_fill(int cx, int cy, int r, uint32_t color) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) gpu_pixel(cx + x, cy + y, color);
}

void gpu_text(int x, int y, const char *s, uint32_t fg) {
    uint8_t bmp[FONT_H * FONT_BPR];
    while (*s) {
        get_font_bitmap(*s, bmp);
        for (int i = 0; i < FONT_H; i++)
            for (int b = 0; b < FONT_BPR; b++) {
                uint8_t bits = bmp[i * FONT_BPR + b];
                for (int j = 0; j < 8; j++) {
                    int pxx = b * 8 + j;
                    if (pxx >= FONT_W) break;
                    if ((bits << j) & 0x80) gpu_pixel(x + pxx, y + i, fg);
                }
            }
        x += FONT_W + 2;
        s++;
    }
}

uint32_t *gpu_buffer(void) {
    return use_direct ? (uint32_t *)(uintptr_t)fb_addr : backbuf;
}

uint32_t gpu_stride(void) {
    return use_direct ? fb_pitch / 4 : fb_w;
}

void gpu_present(void) {
    if(display_is_double_buffered()){
        uint32_t *bb = display_get_backbuffer();
        if(!bb || use_direct) return;
        for (int y = 0; y < vh; y++) {
            uint8_t *dst = (uint8_t *)bb + (uint32_t)(vy + y) * 1920 * 4 + (uint32_t)vx * 4;
            uint8_t *src = (uint8_t *)&backbuf[(uint32_t)(vy + y) * fb_w + (uint32_t)vx];
            memcpy(dst, src, (uint32_t)vw * 4);
        }
        display_dirty(vx, vy, vw, vh);
        display_present();
        return;
    }
    if (use_direct) return;
    for (int y = 0; y < vh; y++) {
        uint8_t *dst = (uint8_t *)(uintptr_t)fb_addr + (uint32_t)(vy + y) * fb_pitch + (uint32_t)vx * 4;
        uint8_t *src = (uint8_t *)&backbuf[(uint32_t)(vy + y) * fb_w + (uint32_t)vx];
        memcpy(dst, src, (uint32_t)vw * 4);
    }
}
