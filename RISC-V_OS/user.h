#pragma once
#include "common.h"

__attribute__((noreturn)) void exit(void);
void putchar(char ch);
int getchar(void);
int open(uint32_t inode_no);
int close(int fd);
int read(int fd, void *buf, uint32_t count);
int write(int fd, const void *buf, uint32_t count);