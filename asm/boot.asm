; multiboot specification
MB_ALIGN     equ 1 << 0
MB_MEMINFO   equ 1 << 1
MB_GRAPHICS  equ 1 << 2
MB_MAGIC     equ 0x1BADB002

MB_FLAGS     equ MB_ALIGN | MB_MEMINFO | MB_GRAPHICS
MB_CHECKSUM  equ -(MB_MAGIC + MB_FLAGS)


section .multiboot
align 4
  dd MB_MAGIC
  dd MB_FLAGS
  dd MB_CHECKSUM

  ; Address fields (set to 0 for ELF binaries)
  dd 0, 0, 0, 0, 0

  dd 0 ; mode type = 0 means linear framebuffer, 1 means text
  dd 800 ; width
  dd 600 ; height
  dd 32  ; bit per pixel 

section .bss
align 16
stack_bottom:
  resb 16384
stack_top:


section .text
global _start
extern kernel_main

_start:
  mov esp, stack_top
  push ebx ; pushing multiboot info addr
  call kernel_main

  cli

.hang:
  hlt
  jmp .hang

_start_end:

_start_size equ (_start_end - _start)
.end:

; 4. Explicitly mark the stack as non-executable to remove linker warning
section .note.GNU-stack noalloc noexec nowrite progbits
