#include <display.h>
#include <stdint.h>
#include <stddef.h>
#include <font.h>
#include <string.h>

#define TRACK_X 2
#define TRACK_Y 2
#define CELL_W (FONT_W + TRACK_X)
#define LINE_H (FONT_H + TRACK_Y)

#define MAX_ROWS 256
#define MAX_COLS 256

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;
static uint32_t fg_color = 0xffffffff;
static uint32_t bg_color = 0x0;

static multiboot_info_t* mb_info = NULL;

static char grid[MAX_ROWS][MAX_COLS];
static uint32_t ncols = 0;
static uint32_t vis_rows = 0;
static uint32_t cur_r = 0;
static uint32_t cur_c = 0;
static int view_off = 0;
static uint32_t region_x0 = 0;
static uint32_t region_x1 = 0;
static uint32_t region_top = 0;
static uint32_t region_bot = 0;

void init_display(multiboot_info_t* mb_info_data){
    mb_info = mb_info_data;
    memset(grid, ' ', sizeof(grid));
    cur_r = 0;
    cur_c = 0;
    cursor_x = 0;
    cursor_y = 0;
    view_off = 0;
    display_set_region(0, 0, mb_info->framebuffer_width, mb_info->framebuffer_height);
}

void display_set_region(uint32_t x0, uint32_t top, uint32_t x1, uint32_t bot){
    if (mb_info == NULL) return;
    if (x0 >= x1 || x1 > mb_info->framebuffer_width) return;
    if (top >= bot || bot > mb_info->framebuffer_height) return;
    if (x1 - x0 < CELL_W || bot - top < LINE_H) return;
    region_x0 = x0;
    region_x1 = x1;
    region_top = top;
    region_bot = bot;
    ncols = (x1 - x0) / CELL_W;
    if (ncols == 0) ncols = 1;
    if (ncols > MAX_COLS) ncols = MAX_COLS;
    vis_rows = (bot - top) / LINE_H;
    if (vis_rows == 0) vis_rows = 1;
    cursor_x = x0;
    cursor_y = region_top;
}

void display_set_fg(uint32_t c){ fg_color = c; }
void display_set_bg(uint32_t c){ bg_color = c; }
uint32_t display_width(void){ return mb_info ? mb_info->framebuffer_width : 0; }
uint32_t display_height(void){ return mb_info ? mb_info->framebuffer_height : 0; }
void display_get_cursor(uint32_t *x, uint32_t *y){
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if(mb_info == NULL) return;

    if (x >= mb_info->framebuffer_width || y >= mb_info->framebuffer_height) return;

    uint32_t* pixel_addr = (uint32_t*)(
        (uint8_t*)(uintptr_t)mb_info->framebuffer_addr +
        (y * mb_info->framebuffer_pitch) +
        (x * 4)
    );

    *pixel_addr = color;
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t y = start_y; y < start_y + h; y++) {
        for (uint32_t x = start_x; x < start_x + w; x++) {
            draw_pixel(x, y, color);
        }
    }
}

static void draw_glyph(char c, uint32_t x, uint32_t y){
   uint8_t bitmap_arr[FONT_H * FONT_BPR];
   get_font_bitmap(c, bitmap_arr);
   for (int i = 0; i < FONT_H; i++){
      for (int b = 0; b < FONT_BPR; b++){
         uint8_t bits = bitmap_arr[i * FONT_BPR + b];
         for (int j = 0; j < 8; j++){
            int px = b * 8 + j;
            if (px >= FONT_W) break;
            if ((bits << j) & 0x80){
                draw_pixel(x + px, y + i, fg_color);
            } else {
                draw_pixel(x + px, y + i, bg_color);
            }
         }
      }
   }
}

static uint32_t live_top(void){
    if (cur_r + 1 > vis_rows) return cur_r + 1 - vis_rows;
    return 0;
}

static void render_window(uint32_t top){
    if(mb_info == NULL) return;
    for (uint32_t r = 0; r < vis_rows; r++){
        uint32_t gr = top + r;
        for (uint32_t c = 0; c < ncols; c++){
            char ch = (gr < MAX_ROWS) ? grid[gr][c] : ' ';
            draw_glyph(ch, region_x0 + c * CELL_W, region_top + r * LINE_H);
        }
    }
    cursor_x = region_x0 + cur_c * CELL_W;
    cursor_y = (cur_r >= top) ? region_top + (cur_r - top) * LINE_H : region_top;
}

static void ensure_live(void){
    if (view_off != 0){
        view_off = 0;
        render_window(live_top());
    }
}

void display_scroll_up(void){
    if(mb_info == NULL || vis_rows == 0) return;
    uint32_t bottom = live_top();
    uint32_t page = vis_rows > 1 ? vis_rows - 1 : 1;
    uint32_t off = (uint32_t)view_off + page;
    if (off > bottom) off = bottom;
    if ((int)off == view_off) return;
    view_off = (int)off;
    render_window(bottom - (uint32_t)view_off);
}

void display_scroll_down(void){
    if(mb_info == NULL || vis_rows == 0) return;
    if (view_off == 0) return;
    uint32_t bottom = live_top();
    uint32_t page = vis_rows > 1 ? vis_rows - 1 : 1;
    int off = view_off - (int)page;
    if (off < 0) off = 0;
    view_off = off;
    render_window(bottom - (uint32_t)view_off);
}

static void scroll_up(void){
    if(mb_info == NULL) return;
    uint32_t rw = (region_x1 > region_x0) ? region_x1 - region_x0 : 0;
    uint32_t rh = (region_bot > region_top) ? region_bot - region_top : 0;
    if(LINE_H >= rh || rh == 0 || rw == 0) return;
    uint8_t *base = (uint8_t*)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t pitch = mb_info->framebuffer_pitch;
    for (uint32_t y = region_top; y + LINE_H < region_bot; y += LINE_H)
        memmove(base + y * pitch + region_x0 * 4,
                base + (y + LINE_H) * pitch + region_x0 * 4, rw * 4);
    draw_rect(region_x0, region_bot - LINE_H, rw, LINE_H, bg_color);
}

void draw_char(char c){
    ensure_live();
    if(mb_info == NULL) return;
    grid[cur_r][cur_c] = c;
    draw_glyph(c, cursor_x, cursor_y);
    cur_c++;
    uint32_t new_x = cursor_x + CELL_W;
    if(cur_c >= ncols || new_x > region_x1){
        newline();
    } else {
        cursor_x = new_x;
    }
}

void backspace(){
    ensure_live();
    if(mb_info == NULL) return;
    uint32_t step = CELL_W;
    if(cursor_x >= region_x0 + step && cur_c > 0){
        cursor_x -= step;
        cur_c--;
    } else if(cursor_y >= region_top + LINE_H && cur_r > 0){
        cursor_y -= LINE_H;
        cur_r--;
        cur_c = ncols - 1;
        cursor_x = region_x0 + cur_c * CELL_W;
    } else {
        return;
    }
    grid[cur_r][cur_c] = ' ';
    draw_rect(cursor_x, cursor_y, FONT_W, FONT_H, bg_color);
}

void draw_string(char *str){
    while(*str){
        if(*str == '\n'){
            newline();
        }else if(*str == '\b'){
            backspace();
        }else{
            draw_char(*str);
        }
        str++;
    }
}

void set_cursor(uint32_t x, uint32_t y){
    cursor_x = x;
    cursor_y = y;
}

void display_text_at(uint32_t x, uint32_t y, char *s){
    if(mb_info == NULL) return;
    while(*s){
        if(*s != '\n') draw_glyph(*s, x, y);
        x += CELL_W;
        s++;
    }
}

void move_cursor_text(){
    if(mb_info == NULL) return;
    cur_c++;
    uint32_t new_x = cursor_x + CELL_W;
    if(cur_c >= ncols || new_x > region_x1){
        newline();
    } else {
        cursor_x = new_x;
    }
}

void newline(){
    ensure_live();
    cursor_x = region_x0;
    cur_c = 0;
    cur_r++;
    if(cur_r >= MAX_ROWS){
        memmove(grid, grid[1], (MAX_ROWS - 1) * MAX_COLS);
        memset(grid[MAX_ROWS - 1], ' ', MAX_COLS);
        cur_r = MAX_ROWS - 1;
    }
    cursor_y = cursor_y + LINE_H;
    while(mb_info != NULL && cursor_y + FONT_H > region_bot){
        uint32_t rh = (region_bot > region_top) ? region_bot - region_top : 0;
        if (LINE_H >= rh || rh == 0) { cursor_y = region_top; break; }
        scroll_up();
        cursor_y = cursor_y - LINE_H;
    }
}

void display_clear(void){
    if(mb_info == NULL) return;
    memset(grid, ' ', sizeof(grid));
    cur_r = 0;
    cur_c = 0;
    view_off = 0;
    draw_rect(region_x0, region_top, region_x1 - region_x0, region_bot - region_top, bg_color);
    cursor_x = region_x0;
    cursor_y = region_top;
}
