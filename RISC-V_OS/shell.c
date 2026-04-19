#include "user.h"

void main(void) {
    char buf[128];
    while (1) {
        printf("> ");
        int i = 0;
        while (1) {
            char c = getchar();
            putchar(c); // echo
            if (c == '\r') {
                putchar('\n');
                break;
            }
            if (i < (int)sizeof(buf) - 1)
                buf[i++] = c;
        }
        buf[i] = '\0';

        if (strcmp(buf, "hello") == 0)
            printf("Hello world from shell!\n");
        else if (strcmp(buf, "exit") == 0)
            exit();
        else
            printf("unknown command: %s\n", buf);
    }
}
