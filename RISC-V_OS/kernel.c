typedef unsigned char unit8_t;
typedef unsigned int unit32_t;
typedef unit32_t size_t;

extern char __bss[], __bss_end[], __stack_top[];

void *memset(void *buf, char c, size_t n) {
    unit8_t *p = (unit8_t *)buf;
    while (n--)
        *p++ = c;
    return buf;
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    for (;;);
}

__attribute__((section(".text.book")));
__attribute__((naked));
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // set the stack pointer bitch
        "j kernel_main\n" // jump ro the kernel main function
        :
        :  [stack_top] "r" (__stack_top) // pass the stack top address as as %[stack_top]
    );
}