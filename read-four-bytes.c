#include<errno.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

void check_error(int value, const char* message) {
    if (value != -1) {
        return;
    }
    int error = errno;
    perror(message);
    exit(error);
}

// what the hell is the type of argv ??!
// the easiest way to descripe it is to call it an array of C strings
int main(int args, char* argv[]) {
    if (args != 2) {
        return EINVAL;
    }

    int fd = open(argv[1], O_RDONLY);
    check_error(fd, "open");

    char buffer[4];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    check_error(bytes_read, "read");

    for (int i = 0; i < bytes_read; ++i) {
        char byte = buffer[i];
        if (byte > 31 && byte < 127) { // if so, it would be smth you can actually type on a keyboard
            printf("Byte %d: %c\n", i, byte); // so we print it like a charachter 
        }
        else {
            printf("Byte %d: 0x%02hhx\n", i, byte); // otherwise it would be printed as a hex charachter 
        }
    } 
}