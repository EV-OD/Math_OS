global gdt_load

gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax             ; data segment
    mov es, ax             ; extra segment
    mov fs, ax             ; general-purpose segment
    mov gs, ax             ; general-purpose segment
    mov ss, ax             ; stack segment

   jmp 0x08:.flush_cs


.flush_cs:
    ; cs is now 0x08, all other segment registers are 0x10.
    ; The CPU is fully using our new GDT.
    ret

section .note.GNU-stack noalloc noexec nowrite progbits

