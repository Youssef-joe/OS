[bits 32]

section .text
global load_idt
global enable_interrupts
global disable_interrupts
global isr_stub_table
extern interrupt_handler

; IDT loader: teaching the CPU where to send its complaints.
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; Interrupt switches: the big red buttons for "yes, really" and "absolutely not."
enable_interrupts:
    sti
    ret

disable_interrupts:
    cli
    ret

; Stub generator: because 48 tiny entrances are easier than one giant panic.
%macro ISR_STUB 1
global isr_stub_%1
isr_stub_%1:
    push dword 0
    push dword %1
    jmp interrupt_common_stub
%endmacro

; Shared interrupt prologue/epilogue: the assembly equivalent of putting on a uniform.
interrupt_common_stub:
    pusha
    push esp
    call interrupt_handler
    add esp, 4
    popa
    add esp, 8
    iretd

; The first 48 vectors get actual stubs, and the rest are left as future homework.
%assign i 0
%rep 48
ISR_STUB i
%assign i i + 1
%endrep

section .rodata
; Table of stub addresses: a tiny phonebook for the CPU.
isr_stub_table:
%assign i 0
%rep 48
    dd isr_stub_%+i
%assign i i + 1
%endrep
