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
        else if (strcmp(buf, "open") == 0) {
            int fd = open(0);
            printf("opened fd %d\n", fd);
        }
        else if (strcmp(buf, "write") == 0) {
            int fd = open(0);
            char data[] = "Hello disk!";
            int n = write(fd, data, sizeof(data) - 1);
            printf("wrote %d bytes\n", n);
            close(fd);
        }
        else if (strcmp(buf, "read") == 0) {
            int fd = open(0);
            char rbuf[20];
            int n = read(fd, rbuf, 20);
            rbuf[n] = '\0';
            printf("read %d bytes: %s\n", n, rbuf);
            close(fd);
        }
        else
            printf("unknown command: %s\n", buf);
    }
}
