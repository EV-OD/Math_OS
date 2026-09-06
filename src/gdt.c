#include <gdt.h>
#include <stdint.h>

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdtr_32 gdt_ptr;
static struct tss_entry tss;

static uint8_t kernel_stack[8192] __attribute__((aligned(16)));

void add_entry(int i, uint32_t base, uint32_t limit, uint8_t access_byte, uint8_t flags){
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].base_low = base & 0xFFFF;
    gdt[i].base_mid = (base >> 16) & 0xFF;
    gdt[i].access_byte = access_byte;
    gdt[i].limit_flags = ((flags & 0xF) << 4) | ((limit >> 16) & 0xF);
    gdt[i].base_high = (base >> 24) & 0xFF;
}

void set_kernel_stack(uint32_t esp0){
    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.esp0 = esp0;
}

void switch_to_user_mode(uint32_t user_func, uint32_t user_stack){
    set_kernel_stack((uint32_t)(kernel_stack + sizeof(kernel_stack)));
    enter_user_mode(user_func, user_stack);
}

void init_gdt(){
    add_entry(0, 0, 0, 0, 0);
    add_entry(1, 0, 0xFFFFF, KERNEL_CODE_SEGMENT_ACCESS_BYTE, KERNEL_FLAG);
    add_entry(2, 0, 0xFFFFF, KERNEL_DATA_SEGMENT_ACCESS_BYTE, KERNEL_FLAG);
    add_entry(3, 0, 0xFFFFF, USER_CODE_SEGMENT_ACCESS_BYTE, KERNEL_FLAG);
    add_entry(4, 0, 0xFFFFF, USER_DATA_SEGMENT_ACCESS_BYTE, KERNEL_FLAG);

    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    add_entry(5, tss_base, tss_limit, TSS_ACCESS_BYTE, TSS_FLAG);

    for (unsigned int i = 0; i < sizeof(tss); i++)
        ((uint8_t*)&tss)[i] = 0;
    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.esp0 = (uint32_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    gdt_ptr.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint32_t)&gdt;
    gdt_load((unsigned int)&gdt_ptr);
    tss_flush();
}
