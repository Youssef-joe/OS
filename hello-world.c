
#include <sys/types.h>
#include <unistd.h>

int main() {
    ssize_t write(int fd, const void *buf, size_t count);
    // that's the signature of the write function, it takes a file descriptor, a pointer to a buffer and the number of bytes to write from that buffer
    // writes bytes from a byte array to a file descriptor, and returns the number of bytes written or -1 on error
    // fd - the file descriptor to write to, buf - a pointer to the buffer containing the data to write, count - the number of bytes to write from the buffer
    const char* message = "Hello, World!\n";


    void exit_group(int status);
    // exists the current process and sets an exist status code, the status code is an integer that can be used to indicate success or failure of the process, and can be used by other processes to determine how to handle the exit of this process
    //status - the exit status code, where 0 typically indicates success and non-zero indicates failure

}
// the most basic "Hello World" program would start executing the following:
void _start(void) {
    write(1, "Hello, World!\n", 12); // write the message to the standard output (file descriptor 1)
    exit_group(0); // exit the process with a status code of 0 (indicating success)
}