#ifndef GDT_H
#define GDT_H
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access_byte;
    uint8_t limit_flags;
    uint8_t base_high;
} __attribute__((packed));

struct __attribute__((__packed__)) gdtr_32 {
    uint16_t limit;   // Size of GDT - 1
    uint32_t base;    // Linear address of GDT
};

// access byte
#define KERNEL_CODE_SEGMENT_ACCESS_BYTE 0x9a
#define KERNEL_DATA_SEGMENT_ACCESS_BYTE 0x92

//flags
#define KERNEL_FLAG 0xc

void add_entry(int i, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags);
void init_gdt();
extern void gdt_load(unsigned int gdt_ptr_addr);


#endif
