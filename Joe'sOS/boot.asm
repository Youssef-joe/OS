[org 0x7C00]
[bits 16]

KERNEL_LOAD_ADDR equ 0x1000

%include "build/kernel_sectors.inc"

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, boot_message
    call print_string

    mov bx, KERNEL_LOAD_ADDR
    mov byte [current_sector], 2
    mov byte [current_head], 0
    mov byte [current_track], 0
    mov cx, KERNEL_SECTORS

load_kernel:
    cmp cx, 0
    je enter_protected_mode

    mov ah, 0x02
    mov al, 0x01
    mov ch, [current_track]
    mov cl, [current_sector]
    mov dh, [current_head]
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    add bx, 512
    dec cx

    inc byte [current_sector]
    cmp byte [current_sector], 19
    jl load_kernel

    mov byte [current_sector], 1
    inc byte [current_head]
    cmp byte [current_head], 2
    jl load_kernel

    mov byte [current_head], 0
    inc byte [current_track]
    jmp load_kernel

disk_error:
    mov si, disk_error_message
    call print_string
    cli
    hlt
    jmp $

print_string:
    mov ah, 0x0E

.next:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .next

.done:
    ret

enter_protected_mode:
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:protected_mode

[bits 32]
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov eax, KERNEL_LOAD_ADDR
    call eax

halt:
    cli
    hlt
    jmp halt

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ 0x08
DATA_SEG equ 0x10

[bits 16]
boot_message db "Booting Joe's OS...", 0x0D, 0x0A, 0
disk_error_message db "Disk read failed.", 0x0D, 0x0A, 0
boot_drive db 0
current_sector db 0
current_head db 0
current_track db 0

times 510 - ($ - $$) db 0
dw 0xAA55
