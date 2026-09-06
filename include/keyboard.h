#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdint.h>
#include <idt.h>

#define READ_ADDR 0x60

void keyboard_handler(struct cpu_state *cpu, struct stack_state *stack, unsigned int interrupt);
void init_keyboard();
#endif
