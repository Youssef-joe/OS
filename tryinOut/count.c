#include<stdio.h>
#include<unistd.h>


// static means that i can only access that global variable from the C file cannot 
// access is from outside the C file 
// so if you have another C file and made a global variable there it would be different
// it would be diff var not same one (another memory location)
// this one is only accessebile in this file 
static int global = 0;

// uint32_t value;
// __asm__(this defines the inline assembly) __volatile__(this tells the compiler not to optimize the assembly code)("csrr %0, sepc" : "=r"(value));


int main(void) {
    int local = 0;
    while (1) {
        ++local;
        ++global;
        printf("local = %d, global = %d\n", local, global);
        sleep(1);
    }
    return 0;
}
