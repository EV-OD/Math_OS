
global idt_load
extern interrupt_handler


idt_load:
    mov eax, [esp + 4]   ; address of idt entry
    lidt [eax]
    ret


%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0  ; dummy error code
    push dword %1 ; isr number
    jmp isr_handler
%endmacro


%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1 ; isr number
    jmp isr_handler
%endmacro

%macro IRQ 2
irq%1:
global irq%1
    push dword 0          ; dummy error code
    push dword %2         ; interrupt number (remapped vector)
    jmp isr_handler
%endmacro

isr_handler:
    pusha
    mov eax, esp;
    push dword [eax + 32] ; int number
    lea ecx, [eax + 32]   ; stack state pointer
    push ecx
    lea ecx, [eax]        ; cpu state
    push ecx

    call interrupt_handler
    add esp, 12           ; we pushed 3 data (3*4 = 12), popping them
    popa
    add esp, 8
    iret

; CPU exceptions without error code
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_NOERR 9
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; CPU exceptions with hardware-pushed error code
ISR_ERR 8
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_ERR 17

; Hardware IRQs 
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

section .note.GNU-stack noalloc noexec nowrite progbits
