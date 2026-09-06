
#include <stdint.h>
#include <stddef.h>
#include <multiboot.h>
#include <display.h>
#define STB_SPRINTF_NOFLOAT 
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>

void kernel_main(uint32_t* multiboot_info_addr){

  multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
  init_gdt();
  init_idt();
  init_pic();
  init_keyboard();
  init_display(mb_info);
  draw_rect(0, 0, mb_info->framebuffer_width/2, mb_info->framebuffer_height/2, 0x000E1E2E);
  char name[128];
  stbsp_snprintf(name, sizeof(name), "Name: %s\nTesting", "rabin");
  draw_string(name);
  __asm__ volatile("sti");
  while(1) __asm__ volatile("hlt");
  // while (1) {}
}
