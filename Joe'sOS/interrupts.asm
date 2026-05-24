[bits 32]

section .text
global load_idt
global enable_interrupts
global disable_interrupts
global isr_stub_table
extern interrupt_handler

load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

enable_interrupts:
    sti
    ret

disable_interrupts:
    cli
    ret

%macro ISR_STUB 1
global isr_stub_%1
isr_stub_%1:
    push dword 0
    push dword %1
    jmp interrupt_common_stub
%endmacro

interrupt_common_stub:
    pusha
    push esp
    call interrupt_handler
    add esp, 4
    popa
    add esp, 8
    iretd

%assign i 0
%rep 48
ISR_STUB i
%assign i i + 1
%endrep

section .rodata
isr_stub_table:
%assign i 0
%rep 48
    dd isr_stub_%+i
%assign i i + 1
%endrep
