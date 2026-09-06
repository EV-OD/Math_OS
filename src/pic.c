#include <pic.h>
#include <stdint.h>
#include <io.h>

void init_pic(){


    // ICW1
    outb(PIC1_COMMAND, ICW1);  
    io_wait();

    outb(PIC2_COMMAND, ICW1);  
    io_wait();


    // vector remap
    remap_pic();

    outb(PIC1_DATA, 1 << CASCADE_IRQ);        // ICW3: tell Master PIC that there is a slave PIC at IRQ2
	io_wait();
	outb(PIC2_DATA, CASCADE_IRQ);             // ICW3: tell Slave PIC its cascade identity
	io_wait();

    outb(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)

	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();
}

void enable_keyboard(){
    IRQ_clear_mask(1);
}

void remap_pic(){
    outb(PIC1_DATA,0x20);                 // ICW2: Master PIC vector offset
	io_wait();
	outb(PIC2_DATA,0x28);                 // ICW2: Slave PIC vector offset
	io_wait();
}

void IRQ_set_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);        
}

void IRQ_clear_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);        
}

void pic_acknowledge(unsigned int interrupt)
{
    /* Only acknowledge interrupts in the remapped range. */
   if (interrupt < 0x20 || interrupt >= 0x28 + 8) {
        return;
    }

    /* If the interrupt came from the slave, acknowledge it first. */
    if (interrupt >= 0x28) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

void PIC_sendEOI(uint8_t irq)
{
	if(irq >= 8)
		outb(PIC2_COMMAND,PIC_EOI);
	
	outb(PIC1_COMMAND,PIC_EOI);
}
