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
    uint16_t limit;
    uint32_t base;
};

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

#define GDT_ENTRIES 6

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR   0x18
#define USER_DATA_SELECTOR   0x20
#define TSS_SELECTOR         0x28

#define KERNEL_CODE_SEGMENT_ACCESS_BYTE 0x9a
#define KERNEL_DATA_SEGMENT_ACCESS_BYTE 0x92
#define USER_CODE_SEGMENT_ACCESS_BYTE   0xfa
#define USER_DATA_SEGMENT_ACCESS_BYTE   0xf2
#define TSS_ACCESS_BYTE                 0x89

#define KERNEL_FLAG 0xc
#define TSS_FLAG    0x0

void add_entry(int i, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags);
void init_gdt();
void set_kernel_stack(uint32_t esp0);
void switch_to_user_mode(uint32_t user_func, uint32_t user_stack);

extern void gdt_load(unsigned int gdt_ptr_addr);
extern void tss_flush();
extern void enter_user_mode(uint32_t func, uint32_t stack);

#endif
