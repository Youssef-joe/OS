#pragma once
#include "common.h"
#define PROCS_MAX 8 // that is the maximum number of processes as it shown
#define PROC_UNUSED 0 // unused process control structure 
#define PROC_RUNNABLE 1 // if you are that dumb -> that means the runnable process 
#define SATP_SV32 (1u << 31)
#define PAGE_V (1 << 0) // "valid" bit (entry is enabled)
#define PAGE_R (1 << 1) //Readable
#define PAGE_W (1 << 2) // Writeable
#define PAGE_X (1 << 3) //Executable
#define PAGE_U (1 << 4) //User (accessible in user mode)
#define USER_BASE 0x1000000
#define SSTATUS_SPIE (1 << 5)

struct process {
    int pid; // you're not dumb now as i didn't recognize it at first, that is the process id
    int state; // that is the process stat (if ykyk) ;)
    vaddr_t sp; // that iss the stack pointer (a great topic that u can search about)
    uint32_t *page_table; // here we're adding the page table. this will be a pointer to 1st-level page table
    uint8_t stack[8192]; // kernel stack (if you're jobless search for it)
};

struct  sbiret
{
    /* data */long error;
              long value;
};


 struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                                          \
    ({                                                                         \
        unsigned long __tmp;                                                   \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));                  \
        __tmp;                                                                 \
    })

#define WRITE_CSR(reg, value)                                                  \
    do {                                                                       \
        uint32_t __tmp = (value);                                              \
        __asm__ __volatile__("csrw " #reg ", %0" ::"r"(__tmp));                \
    } while (0)