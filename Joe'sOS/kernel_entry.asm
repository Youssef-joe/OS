[bits 32]
section .text
global _start
extern kernel_main

; Entry point: the kernel's front door, minus the welcome mat.
_start:
    call kernel_main

; If control ever falls through, we prefer the dramatic option: endless nap mode.
.halt:
    cli
    hlt
    jmp .halt
