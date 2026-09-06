#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    
    // --- VBE Control & Mode Structures ---
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;

    // --- Linear Framebuffer Fields (Multiboot v1) ---
    uint64_t framebuffer_addr;   // 64-bit physical RAM base address of video memory
    uint32_t framebuffer_pitch;  // Number of bytes per horizontal line (scanline)
    uint32_t framebuffer_width;  // Screen width in pixels
    uint32_t framebuffer_height; // Screen height in pixels
    uint8_t  framebuffer_bpp;    // Bits per pixel (e.g., 32)
    uint8_t  framebuffer_type;   // 1 = Indexed color, 1 = Direct RGB color
} __attribute__((packed)) multiboot_info_t;


#endif /* MULTIBOOT_H */
