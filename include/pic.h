#ifndef PIC_H
#define PIC_H
#include <stdint.h>
#include <io.h>
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21

#define PIC2_COMMAND 0xa0
#define PIC2_DATA 0xa1


#define ICW1 0x11
#define ICW4_8086	0x01
#define CASCADE_IRQ 2

#define ENABLE_KEYBOARD 0xFD
#define PIC_EOI		0x20		/* End-of-interrupt command code */

void PIC_sendEOI(uint8_t irq);
void init_pic();
void remap_pic();
void pic_acknowledge(unsigned int interrupt);

void IRQ_set_mask(uint8_t IRQline);
void IRQ_clear_mask(uint8_t IRQline);

void enable_keyboard();
#endif
