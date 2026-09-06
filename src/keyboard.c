#include <keyboard.h>
#include <pic.h>
#include <idt.h>
#include <display.h>
#include <io.h>
#include <string.h>



void keyboard_handler(struct cpu_state *cpu, struct stack_state *stack, unsigned int interrupt){
    (void)cpu;
    (void)stack;
    (void)interrupt;
    uint8_t sc = inb(READ_ADDR);
    char buf[32];
    sprintf(buf, "%x ", sc);
    draw_string(buf);
}

void init_keyboard(){
     register_interrupt_handler(33, keyboard_handler);
     enable_keyboard();
}
