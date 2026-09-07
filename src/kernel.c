
#include <stdint.h>
#include <stddef.h>
#include <multiboot.h>
#include <display.h>
#include <stdio.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <keyboard.h>
#include <log.h>
#include <process.h>
#include <shell.h>
#include <user_progs.h>
#include <gpu.h>
#include <ui.h>
#include <tmux.h>

static void kernel_init(multiboot_info_t* mb_info){
  log_init();
  log_info("boot: kernel start");
  init_gdt();
  log_debug("gdt ready");
  init_idt();
  log_debug("idt ready");
  init_pic();
  log_debug("pic ready");
  init_keyboard();
  log_debug("keyboard ready");
  init_display(mb_info);
  ui_splash();
  log_info("drivers ready fb=%dx%d", mb_info->framebuffer_width, mb_info->framebuffer_height);
  gpu_init(mb_info->framebuffer_addr, mb_info->framebuffer_pitch,
           mb_info->framebuffer_width, mb_info->framebuffer_height);
  log_debug("gpu ready %dx%d", gpu_screen_width(), gpu_screen_height());
  process_init();
  timer_init(100);
  log_debug("timer ready");
  ui_chrome();
}

void kernel_main(uint32_t* multiboot_info_addr){

  multiboot_info_t* mb_info = (multiboot_info_t*)multiboot_info_addr;
  kernel_init(mb_info);
  process_start();
  log_info("sched: started");
  __asm__ volatile("sti");
  tmux_run(mb_info);
  while(1) __asm__ volatile("hlt");
}
