global irq0_entry
extern timer_pick
extern fpu_cur
extern fpu_next

; Preemptive timer entry. Forged stacks (see process.c) match the
; generic isr_handler layout: [pusha regs][err][int][eip][cs][eflags](+[esp][ss])
irq0_entry:
    push dword 0
    push dword 32
    pusha
    mov eax, esp      ; cpu_state* == current esp (points at saved regs)
    push eax          ; arg: cur_esp
    call timer_pick   ; eax = next proc esp
    add esp, 4        ; drop arg (still on OLD stack; ret already popped)
    mov ebx, eax      ; stash next esp
    mov eax, [fpu_cur]
    fxsave [eax]
    mov eax, [fpu_next]
    fxrstor [eax]
    mov esp, ebx      ; switch to next proc
    popa
    add esp, 8        ; drop err + int number
    iret

section .note.GNU-stack noalloc noexec nowrite progbits
