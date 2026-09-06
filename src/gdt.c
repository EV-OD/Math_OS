#include <gdt.h>
#include <stdint.h>

#define GDT_ENTRY_NUM 3
static struct gdt_entry gdt[GDT_ENTRY_NUM];
static struct gdtr_32 gdt_ptr;


void add_entry(int i, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags){
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].base_low = base & 0xFFFF;
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].access_byte = access_byte;
    gdt[i].limit_flags = ((flags & 0xF) << 4) | ((limit >> 16) & 0xF);
    gdt[i].base_high = (base >> 24) & 0xFF;
}


void init_gdt(){
    add_entry(0, 0,0,0,0); // null 
    add_entry(1, 0, 0xFFFFF, KERNEL_CODE_SEGMENT_ACCESS_BYTE, KERNEL_FLAG); 
    add_entry(2, 0, 0xFFFFF, KERNEL_DATA_SEGMENT_ACCESS_BYTE, KERNEL_FLAG); 

    gdt_ptr.limit = (sizeof(struct gdt_entry) * GDT_ENTRY_NUM) - 1;
    gdt_ptr.base = (uint32_t)&gdt;
    gdt_load((unsigned int)&gdt_ptr);
}



