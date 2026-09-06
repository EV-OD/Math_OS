global gdt_load
global tss_flush
global enter_user_mode

gdt_load:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush_cs

.flush_cs:
    ret

tss_flush:
    mov ax, 0x28
    ltr ax
    ret

; void enter_user_mode(uint32_t func, uint32_t stack)
enter_user_mode:
    cli
    mov eax, [esp + 4]   ; func (eip)
    mov ebx, [esp + 8]   ; user stack (esp)

    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push dword 0x20      ; ss
    push ebx             ; esp
    pushfd               ; eflags
    pop ecx
    or ecx, 0x200        ; enable IF
    push ecx             ; eflags with IF
    push dword 0x18      ; cs (user code, RPL 3)
    push eax             ; eip
    iret

section .note.GNU-stack noalloc noexec nowrite progbits
