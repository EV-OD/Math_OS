#include <display.h>
#include <stdint.h>
#include <stddef.h>
#include <font_8x16.h>

#define CHAR_OFFSET_X 5
#define CHAR_OFFSET_Y 2

static uint32_t cursor_x = 0;
static uint32_t cursor_y= 0;
static uint32_t fg_color = 0xffffffff;
static uint32_t bg_color = 0x0;

static multiboot_info_t* mb_info = NULL;

void init_display(multiboot_info_t* mb_info_data){
    mb_info = mb_info_data;
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



void draw_char(char c){

   uint8_t *bitmap_arr = NULL;
   get_bitmap_8x16(c, bitmap_arr); 
   for (int i = 0; i < 16; i++){
      uint8_t bitmap_item = bitmap_arr[i];
      for (int j = 0; j < 8;j++){
          if ((bitmap_item << j) & 0x80){
              draw_pixel(cursor_x +  j, cursor_y +  i, fg_color);  
          } else {
              draw_pixel(cursor_x + j, cursor_y + i, bg_color);  
          }
      }
   }
   move_cursor_text();
}

void draw_string(char *str){
    while(*str){
        if(*str != '\n'){
            draw_char(*str);
        }else{
            newline();
        }
        str++;
    }
}

void set_cursor(uint32_t x, uint32_t y){
    cursor_x = x;
    cursor_y = y;
}

void move_cursor_text(){
    uint32_t new_x = cursor_x + CHAR_OFFSET_X + 8;
    if(new_x > mb_info->framebuffer_width){
        new_x = 0;
        cursor_y = cursor_y + CHAR_OFFSET_Y + 16; 
    }

    cursor_x = new_x;
}

void newline(){
    cursor_x = 0;
    cursor_y = cursor_y + CHAR_OFFSET_Y + 16;
}
