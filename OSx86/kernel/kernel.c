void kmain() {
    volatile unsigned short *vga = (volatile unsigned short*)0xB8000;
    const char *msg = "Hello from OSx86!";

    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i] = (unsigned short)(0x0700 | (unsigned char)msg[i]);
        // 0x07 = white on black, stored in high byte
        // character stored in low byte
    }

    while (1) {}
}
