#include <display.h>
#include <stdint.h>
#include <stddef.h>
#include <font.h>
#include <string.h>
#include <log.h>

#define TRACK_X 2
#define TRACK_Y 2
#define CELL_W (FONT_W + TRACK_X)
#define LINE_H (FONT_H + TRACK_Y)

#define MAX_ROWS 256
#define MAX_COLS 256

typedef struct {
    char grid[MAX_ROWS][MAX_COLS];
    uint32_t fg_grid[MAX_ROWS][MAX_COLS];
    uint32_t bg_grid[MAX_ROWS][MAX_COLS];
    uint32_t ncols;
    uint32_t vis_rows;
    uint32_t cur_r;
    uint32_t cur_c;
    int view_off;
    uint32_t region_x0;
    uint32_t region_x1;
    uint32_t region_top;
    uint32_t region_bot;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg;
    uint32_t bg;
    int inited;
} console_t;

static console_t consoles[DISPLAY_MAX_CONSOLES];
static int cur_id = 0;
static console_t *cur = &consoles[0];
static multiboot_info_t* mb_info = NULL;
static uint32_t *ui_backbuf = 0;
static int ui_double_buffered = 0;
static uint32_t ui_backbuffer_storage[1920*1080];
static int d_empty = 1;
static uint32_t dx0, dy0, dx1, dy1;

void display_enable_double_buffer(int enable){
    ui_double_buffered = enable;
    if(enable) ui_backbuf = ui_backbuffer_storage;
    else ui_backbuf = 0;
}
void display_dirty(int x, int y, int w, int h){
    if(!mb_info || w<=0 || h<=0) return;
    uint32_t W = mb_info->framebuffer_width, H = mb_info->framebuffer_height;
    if(x<0){ w+=x; x=0; }
    if(y<0){ h+=y; y=0; }
    if((uint32_t)x>=W || (uint32_t)y>=H) return;
    uint32_t x1=(uint32_t)x+(uint32_t)w, y1=(uint32_t)y+(uint32_t)h;
    if(x1>W) x1=W;
    if(y1>H) y1=H;
    if(d_empty){ dx0=x; dy0=y; dx1=x1; dy1=y1; d_empty=0; }
    else{
        if((uint32_t)x<dx0) dx0=x;
        if((uint32_t)y<dy0) dy0=y;
        if(x1>dx1) dx1=x1;
        if(y1>dy1) dy1=y1;
    }
}
void display_present(void){
    if(!ui_double_buffered || !mb_info || !ui_backbuf) return;
    if(d_empty) return;
    uint32_t *fb = (uint32_t*)(uintptr_t)mb_info->framebuffer_addr;
    uint32_t pitch = mb_info->framebuffer_pitch;
    for(uint32_t y=dy0;y<dy1;y++){
        memcpy((uint8_t*)fb + y*pitch + dx0*4, (uint8_t*)ui_backbuf + y*1920*4 + dx0*4, (dx1-dx0)*4);
    }
    d_empty=1;
}
uint32_t *display_get_backbuffer(void){ return ui_backbuf; }
int display_is_double_buffered(void){ return ui_double_buffered; }
void init_display(multiboot_info_t* mb_info_data){
    mb_info = mb_info_data;
    ui_backbuf = ui_backbuffer_storage;
    ui_double_buffered = 0;
    for (int i = 0; i < DISPLAY_MAX_CONSOLES; i++) {
        memset(consoles[i].grid, ' ', sizeof(consoles[i].grid));
        for(int r=0;r<MAX_ROWS;r++) for(int c=0;c<MAX_COLS;c++){ consoles[i].fg_grid[r][c]=0xE8ECF5; consoles[i].bg_grid[r][c]=0x0B1020; }
        consoles[i].cur_r = 0;
        consoles[i].cur_c = 0;
        consoles[i].view_off = 0;
        consoles[i].fg = 0xE8ECF5;
        consoles[i].bg = 0x0B1020;
        consoles[i].inited = 0;
    }
    cur_id = 0;
    cur = &consoles[0];
    cur->fg = 0xffffffff;
    cur->bg = 0x0;
    display_set_region(0, 0, mb_info->framebuffer_width, mb_info->framebuffer_height);
    cur->inited = 1;
}

void display_select(int id){
    if (id < 0 || id >= DISPLAY_MAX_CONSOLES) return;
    if (id != cur_id) log_debug("ui: select console %d->%d", cur_id, id);
    cur_id = id;
    cur = &consoles[id];
}

int display_current(void){ return cur_id; }

void display_create(int id, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1){
    if (id < 0 || id >= DISPLAY_MAX_CONSOLES) return;
    display_select(id);
    memset(cur->grid, ' ', sizeof(cur->grid));
    for(int r=0;r<MAX_ROWS;r++) for(int c=0;c<MAX_COLS;c++){ cur->fg_grid[r][c]=0xE8ECF5; cur->bg_grid[r][c]=0x0B1020; }
    cur->cur_r = 0;
    cur->cur_c = 0;
    cur->view_off = 0;
    cur->fg = 0xE8ECF5;
    cur->bg = 0x0B1020;
    cur->inited = 1;
    display_set_region(x0, y0, x1, y1);
    display_clear();
}

void display_render(int id){
    if (id < 0 || id >= DISPLAY_MAX_CONSOLES) return;
    if (!consoles[id].inited) return;
    int prev = cur_id;
    display_select(id);
    uint32_t top = 0;
    if (cur->cur_r + 1 > cur->vis_rows) top = cur->cur_r + 1 - cur->vis_rows;
    if (cur->view_off) {
        if ((uint32_t)cur->view_off > top) cur->view_off = top;
        top -= cur->view_off;
    }
    for (uint32_t r = 0; r < cur->vis_rows; r++) {
        uint32_t gr = top + r;
        for (uint32_t c = 0; c < cur->ncols; c++) {
            char ch = (gr < MAX_ROWS) ? cur->grid[gr][c] : ' ';
            uint8_t bmp[FONT_H * FONT_BPR];
            get_font_bitmap(ch, bmp);
            for (int i = 0; i < FONT_H; i++) {
                for (int b = 0; b < FONT_BPR; b++) {
                    uint8_t bits = bmp[i * FONT_BPR + b];
                    for (int j = 0; j < 8; j++) {
                        int px = b * 8 + j;
                        if (px >= FONT_W) break;
                        uint32_t col = (bits << j) & 0x80 ? cur->fg : cur->bg;
                        draw_pixel(cur->region_x0 + c * CELL_W + px, cur->region_top + r * LINE_H + i, col);
                    }
                }
            }
        }
    }
    cur->cursor_x = cur->region_x0 + cur->cur_c * CELL_W;
    cur->cursor_y = (cur->cur_r >= top) ? cur->region_top + (cur->cur_r - top) * LINE_H : cur->region_top;
    display_select(prev);
}

void display_render_all(void){
    for (int i = 0; i < DISPLAY_MAX_CONSOLES; i++) if (consoles[i].inited) display_render(i);
}

void display_set_region(uint32_t x0, uint32_t top, uint32_t x1, uint32_t bot){
    if (mb_info == NULL) return;
    if (x0 >= x1 || x1 > mb_info->framebuffer_width) return;
    if (top >= bot || bot > mb_info->framebuffer_height) return;
    if (x1 - x0 < CELL_W || bot - top < LINE_H) return;
    cur->region_x0 = x0;
    cur->region_x1 = x1;
    cur->region_top = top;
    cur->region_bot = bot;
    cur->ncols = (x1 - x0) / CELL_W;
    if (cur->ncols == 0) cur->ncols = 1;
    if (cur->ncols > MAX_COLS) cur->ncols = MAX_COLS;
    cur->vis_rows = (bot - top) / LINE_H;
    if (cur->vis_rows == 0) cur->vis_rows = 1;
    cur->cursor_x = x0;
    cur->cursor_y = top;
}

void display_set_fg(uint32_t c){ cur->fg = c; }
void display_set_bg(uint32_t c){ cur->bg = c; }
uint32_t display_width(void){ return mb_info ? mb_info->framebuffer_width : 0; }
uint32_t display_height(void){ return mb_info ? mb_info->framebuffer_height : 0; }
void display_get_cursor(uint32_t *x, uint32_t *y){
    if (x) *x = cur->cursor_x;
    if (y) *y = cur->cursor_y;
}
void display_get_fg_bg(uint32_t *fg, uint32_t *bg){
    if(fg) *fg=cur->fg;
    if(bg) *bg=cur->bg;
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if(mb_info == NULL) return;
    if (x >= mb_info->framebuffer_width || y >= mb_info->framebuffer_height) return;
    if(ui_double_buffered && ui_backbuf){
        ui_backbuf[y*1920 + x] = color;
        display_dirty((int)x, (int)y, 1, 1);
        return;
    }
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
                draw_pixel(x + px, y + i, cur->fg);
            } else {
                draw_pixel(x + px, y + i, cur->bg);
            }
         }
      }
   }
}

static uint32_t live_top(void){
    if (cur->cur_r + 1 > cur->vis_rows) return cur->cur_r + 1 - cur->vis_rows;
    return 0;
}

static int cursor_on = 0;
static unsigned cursor_blink = 0;

static void cursor_redraw_cell(void){
    if(!mb_info || cur->view_off) return;
    if(cur->cur_r >= MAX_ROWS || cur->cur_c >= MAX_COLS) return;
    char ch = cur->grid[cur->cur_r][cur->cur_c];
    uint32_t fg = cur->fg_grid[cur->cur_r][cur->cur_c];
    uint32_t bg = cur->bg_grid[cur->cur_r][cur->cur_c];
    uint32_t old_fg=cur->fg, old_bg=cur->bg;
    cur->fg=fg; cur->bg=bg;
    draw_glyph(ch, cur->cursor_x, cur->cursor_y);
    cur->fg=old_fg; cur->bg=old_bg;
}

void display_cursor_hide(void){
    if(!cursor_on) return;
    cursor_on = 0;
    cursor_redraw_cell();
}

void display_cursor_tick(void){
    if(!mb_info || cur->view_off){ cursor_on = 0; return; }
    cursor_blink++;
    if((cursor_blink & 31) < 16){
        if(!cursor_on){
            cursor_on = 1;
            draw_rect(cur->cursor_x, cur->cursor_y, FONT_W, FONT_H, cur->fg);
        }
    }else{
        if(cursor_on){
            cursor_on = 0;
            cursor_redraw_cell();
        }
    }
}

static void render_window(uint32_t top){
    if(mb_info == NULL) return;
    cursor_on = 0;
    for (uint32_t r = 0; r < cur->vis_rows; r++){
        uint32_t gr = top + r;
        for (uint32_t c = 0; c < cur->ncols; c++){
            char ch = (gr < MAX_ROWS) ? cur->grid[gr][c] : ' ';
            uint32_t fg = (gr < MAX_ROWS) ? cur->fg_grid[gr][c] : cur->fg;
            uint32_t bg = (gr < MAX_ROWS) ? cur->bg_grid[gr][c] : cur->bg;
            uint32_t old_fg=cur->fg, old_bg=cur->bg;
            cur->fg=fg; cur->bg=bg;
            draw_glyph(ch, cur->region_x0 + c * CELL_W, cur->region_top + r * LINE_H);
            cur->fg=old_fg; cur->bg=old_bg;
        }
    }
    cur->cursor_x = cur->region_x0 + cur->cur_c * CELL_W;
    cur->cursor_y = (cur->cur_r >= top) ? cur->region_top + (cur->cur_r - top) * LINE_H : cur->region_top;
}

static void ensure_live(void){
    if (cur->view_off != 0){
        cur->view_off = 0;
        render_window(live_top());
    }
}

void display_scroll_up(void){
    if(mb_info == NULL || cur->vis_rows == 0) return;
    uint32_t bottom = live_top();
    uint32_t page = cur->vis_rows > 1 ? cur->vis_rows - 1 : 1;
    uint32_t off = (uint32_t)cur->view_off + page;
    if (off > bottom) off = bottom;
    if ((int)off == cur->view_off) return;
    cur->view_off = (int)off;
    render_window(bottom - (uint32_t)cur->view_off);
}

void display_scroll_down(void){
    if(mb_info == NULL || cur->vis_rows == 0) return;
    if (cur->view_off == 0) return;
    uint32_t bottom = live_top();
    uint32_t page = cur->vis_rows > 1 ? cur->vis_rows - 1 : 1;
    int off = cur->view_off - (int)page;
    if (off < 0) off = 0;
    cur->view_off = off;
    render_window(bottom - (uint32_t)cur->view_off);
}

static void scroll_up(void){
    if(mb_info == NULL) return;
    uint32_t rw = (cur->region_x1 > cur->region_x0) ? cur->region_x1 - cur->region_x0 : 0;
    uint32_t rh = (cur->region_bot > cur->region_top) ? cur->region_bot - cur->region_top : 0;
    if(LINE_H >= rh || rh == 0 || rw == 0) return;
    uint8_t *base;
    uint32_t pitch;
    if(ui_double_buffered && ui_backbuf){ base = (uint8_t*)ui_backbuf; pitch = 1920*4; }
    else{ base = (uint8_t*)(uintptr_t)mb_info->framebuffer_addr; pitch = mb_info->framebuffer_pitch; }
    for (uint32_t y = cur->region_top; y + LINE_H < cur->region_bot; y += LINE_H)
        memmove(base + y * pitch + cur->region_x0 * 4,
                base + (y + LINE_H) * pitch + cur->region_x0 * 4, rw * 4);
    display_dirty((int)cur->region_x0, (int)cur->region_top, (int)rw, (int)rh);
    draw_rect(cur->region_x0, cur->region_bot - LINE_H, rw, LINE_H, cur->bg);
}

void draw_char(char c){
    ensure_live();
    if(mb_info == NULL) return;
    cursor_on = 0;
    cur->grid[cur->cur_r][cur->cur_c] = c;
    cur->fg_grid[cur->cur_r][cur->cur_c] = cur->fg;
    cur->bg_grid[cur->cur_r][cur->cur_c] = cur->bg;
    draw_glyph(c, cur->cursor_x, cur->cursor_y);
    cur->cur_c++;
    uint32_t new_x = cur->cursor_x + CELL_W;
    if(cur->cur_c >= cur->ncols || new_x > cur->region_x1){
        newline();
    } else {
        cur->cursor_x = new_x;
    }
}

void backspace(){
    ensure_live();
    if(mb_info == NULL) return;
    if(cursor_on){ cursor_on = 0; cursor_redraw_cell(); }
    uint32_t step = CELL_W;
    if(cur->cursor_x >= cur->region_x0 + step && cur->cur_c > 0){
        cur->cursor_x -= step;
        cur->cur_c--;
    } else if(cur->cursor_y >= cur->region_top + LINE_H && cur->cur_r > 0){
        cur->cursor_y -= LINE_H;
        cur->cur_r--;
        cur->cur_c = cur->ncols - 1;
        cur->cursor_x = cur->region_x0 + cur->cur_c * CELL_W;
    } else {
        return;
    }
    cur->grid[cur->cur_r][cur->cur_c] = ' ';
    cur->fg_grid[cur->cur_r][cur->cur_c] = cur->fg;
    cur->bg_grid[cur->cur_r][cur->cur_c] = cur->bg;
    draw_rect(cur->cursor_x, cur->cursor_y, FONT_W, FONT_H, cur->bg);
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
    cur->cursor_x = x;
    cur->cursor_y = y;
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
    cur->cur_c++;
    uint32_t new_x = cur->cursor_x + CELL_W;
    if(cur->cur_c >= cur->ncols || new_x > cur->region_x1){
        newline();
    } else {
        cur->cursor_x = new_x;
    }
}

void newline(){
    ensure_live();
    if(cursor_on){ cursor_on = 0; cursor_redraw_cell(); }
    cur->cursor_x = cur->region_x0;
    cur->cur_c = 0;
    cur->cur_r++;
    if(cur->cur_r >= MAX_ROWS){
        memmove(cur->grid, cur->grid[1], (MAX_ROWS - 1) * MAX_COLS);
        memset(cur->grid[MAX_ROWS - 1], ' ', MAX_COLS);
        cur->cur_r = MAX_ROWS - 1;
    }
    cur->cursor_y = cur->cursor_y + LINE_H;
    while(mb_info != NULL && cur->cursor_y + FONT_H > cur->region_bot){
        uint32_t rh = (cur->region_bot > cur->region_top) ? cur->region_bot - cur->region_top : 0;
        if (LINE_H >= rh || rh == 0) { cur->cursor_y = cur->region_top; break; }
        scroll_up();
        cur->cursor_y = cur->cursor_y - LINE_H;
    }
}

void display_clear(void){
    if(mb_info == NULL) return;
    cursor_on = 0;
    memset(cur->grid, ' ', sizeof(cur->grid));
    for(int r=0;r<MAX_ROWS;r++) for(int c=0;c<MAX_COLS;c++){ cur->fg_grid[r][c]=cur->fg; cur->bg_grid[r][c]=cur->bg; }
    cur->cur_r = 0;
    cur->cur_c = 0;
    cur->view_off = 0;
    draw_rect(cur->region_x0, cur->region_top, cur->region_x1 - cur->region_x0, cur->region_bot - cur->region_top, cur->bg);
    cur->cursor_x = cur->region_x0;
    cur->cursor_y = cur->region_top;
}
