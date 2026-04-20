#include "kernel.h"
#include "common.h"
#define PANIC(fmt, ...)                                                        \
    do {                                                                       \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);  \
        while (1) {}                                                           \
    } while (0)
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

void switch_context(uint32_t *prev_sp, uint32_t *next_sp);
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);
void putchar(char ch);
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid);

extern struct process procs[];
extern struct process *current_proc;
extern struct process *idle_proc;
struct block_device *disk;

// ↓ __attribute__((naked)) is very important!
__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]        \n"
        "csrw sstatus, %[sstatus]  \n"
        "sret                      \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE)
    );
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n"
        "j kernel_main\n"
        :
        : [stack_top] "r" (__stack_top)
    );
}
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrrw sp, sscratch, sp\n"  /* sp=kernel stack top, sscratch=user sp */
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"
        "csrr a0, sscratch\n"        /* a0 = user sp */
        "sw a0,  4 * 30(sp)\n"
        /* restore sscratch to kernel stack top for next trap */
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"
        "mv a0, sp\n"
        "call handle_trap\n"
        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"      /* restore user sp */
        "sret\n"
    );
}
void yield(void) {
    // let's search for a runnable process
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }
    if (next == current_proc) {
        return;
    }

    __asm__ __volatile__(
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        // he said "Don't forget the trailing comma!", man i spent a while understanding that shit
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
         [sscratch] "r" ((uint32_t) &next->stack[8192])
        : "memory"
    );

    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

paddr_t alloc_pages(uint32_t n);
int file_open(uint32_t inode_no);
int file_close(int fd);
int file_read(int fd, void *buf, uint32_t count);
int file_write(int fd, const void *buf, uint32_t count);
void handle_syscall(struct trap_frame *f) {
    switch(f->a3) {
        case SYS_EXIT:
            printf("process %d exited\n", current_proc->pid);
            current_proc->state = PROC_EXITED;
            yield();
            PANIC("unreachable");
        case SYS_PUTCHAR:
            putchar(f->a0);
            break;
        case SYS_GETCHAR:
            while (1) {
                struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
                if (ret.error >= 0) {
                    f->a0 = ret.error;
                    break;
                }
                yield();
            }
            break;
        case SYS_DISK_READ:
            {
                void *kbuf = (void *) alloc_pages(1);
                if (disk->read(f->a0, kbuf) == 0) {
                    memcpy((void *)f->a1, kbuf, BLOCK_SIZE);
                    f->a0 = 0;
                } else {
                    f->a0 = -1;
                }
            }
            break;
        case SYS_DISK_WRITE:
            {
                void *kbuf = (void *) alloc_pages(1);
                memcpy(kbuf, (void *)f->a1, BLOCK_SIZE);
                if (disk->write(f->a0, kbuf) == 0) {
                    f->a0 = 0;
                } else {
                    f->a0 = -1;
                }
            }
            break;
        case SYS_FILE_OPEN:
            f->a0 = file_open(f->a0);
            break;
        case SYS_FILE_CLOSE:
            f->a0 = file_close(f->a0);
            break;
        case SYS_FILE_READ:
            f->a0 = file_read(f->a0, (void *)f->a1, f->a2);
            break;
        case SYS_FILE_WRITE:
            f->a0 = file_write(f->a0, (const void *)f->a1, f->a2);
            break;
        default:
            PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}


void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);
    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, spec=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                                : "=r"(a0), "=r"(a1)
                                : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                                : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char ch) {
    sbi_call((long)ch, 0, 0, 0, 0, 0, 1, 1);
}

paddr_t alloc_pages(uint32_t n) { // dynamic memory alloc 3ashan a7na weak af
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE; // 2al ya3ny al security m2ata3a ba3dha w keda

    if (next_paddr > (paddr_t) __free_ram_end) 
        PANIC("out of memory");
    
        memset((void *) paddr, 0, n * PAGE_SIZE);
        return paddr;
}

__attribute__((naked)) void switch_context(uint32_t *prev_sp,
                                           uint32_t *next_sp) {
    __asm__ __volatile__(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"  // We've popped 13 4-byte registers from the stack
        "ret\n"
    );
}

struct process procs[PROCS_MAX];

static char ramdisk[1024 * BLOCK_SIZE]; // 1024 blocks, simple RAM disk

int ramdisk_read(uint32_t block_no, void *buf) {
    if (block_no >= 1024) return -1;
    memcpy(buf, ramdisk + block_no * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int ramdisk_write(uint32_t block_no, const void *buf) {
    if (block_no >= 1024) return -1;
    memcpy(ramdisk + block_no * BLOCK_SIZE, buf, BLOCK_SIZE);
    return 0;
}

static struct inode inodes[INODES_MAX];
static struct file files[FILES_MAX];

int file_open(uint32_t inode_no) {
    if (inode_no >= INODES_MAX) return -1;
    for (int i = 0; i < FILES_MAX; i++) {
        if (files[i].ref == 0) {
            files[i].ref = 1;
            files[i].inode = &inodes[inode_no];
            files[i].offset = 0;
            return i;
        }
    }
    return -1;
}

int file_close(int fd) {
    if (fd < 0 || fd >= FILES_MAX || files[fd].ref == 0) return -1;
    files[fd].ref--;
    return 0;
}

int file_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= FILES_MAX || files[fd].ref == 0) return -1;
    struct file *f = &files[fd];
    uint32_t remaining = f->inode->size - f->offset;
    if (count > remaining) count = remaining;
    uint32_t bytes_read = 0;
    while (bytes_read < count) {
        uint32_t block_idx = f->offset / BLOCK_SIZE;
        if (block_idx >= 10) break; // no more blocks
        uint32_t block_off = f->offset % BLOCK_SIZE;
        uint32_t block_no = f->inode->blocks[block_idx];
        if (block_no == 0) break; // no block
        void *kbuf = (void *) alloc_pages(1);
        disk->read(block_no, kbuf);
        uint32_t to_copy = BLOCK_SIZE - block_off;
        if (to_copy > count - bytes_read) to_copy = count - bytes_read;
        memcpy((char *)buf + bytes_read, (char *)kbuf + block_off, to_copy);
        bytes_read += to_copy;
        f->offset += to_copy;
    }
    return bytes_read;
}

int file_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || fd >= FILES_MAX || files[fd].ref == 0) return -1;
    struct file *f = &files[fd];
    uint32_t bytes_written = 0;
    while (bytes_written < count) {
        uint32_t block_idx = f->offset / BLOCK_SIZE;
        if (block_idx >= 10) break;
        uint32_t block_off = f->offset % BLOCK_SIZE;
        uint32_t block_no = f->inode->blocks[block_idx];
        if (block_no == 0) {
            // allocate new block, say next free
            static uint32_t next_block = 10; // assume blocks 0-9 reserved
            block_no = next_block++;
            f->inode->blocks[block_idx] = block_no;
        }
        void *kbuf = (void *) alloc_pages(1);
        if (block_off > 0 || (count - bytes_written) < BLOCK_SIZE) {
            // read existing
            disk->read(block_no, kbuf);
        }
        uint32_t to_copy = BLOCK_SIZE - block_off;
        if (to_copy > count - bytes_written) to_copy = count - bytes_written;
        memcpy((char *)kbuf + block_off, (char *)buf + bytes_written, to_copy);
        disk->write(block_no, kbuf);
        bytes_written += to_copy;
        f->offset += to_copy;
        if (f->offset > f->inode->size) f->inode->size = f->offset;
    }
    return bytes_written;
}

extern char __kernel_base[];


struct process *create_process(const void *image, size_t image_size) {
    // let's find the unused process control structure
    struct process *proc = NULL;
    int i;
    for (i = 0; i< PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc) 
        PANIC("no free process slots");

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) user_entry;          // ra (we chenged it here)

    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
            paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);

    // map up to stack top (USER_BASE + 0x11000 covers binary + bss + stack)
    uint32_t map_size = align_up(image_size, PAGE_SIZE) + 0x20000; // image + 128KB for bss/stack
    for (uint32_t off = 0; off < map_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        size_t copy_size = 0;
        if (off < image_size) {
            size_t remaining = image_size - off;
            copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;
            memcpy((void *) page, image + off, copy_size);
        }
        map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
    return proc;
} 

// let's start with testing code
void delay(void) {
    for (int i = 0; i < 30000000; i++) {
        __asm__ __volatile__("nop"); // jus jus jus do nothing 
    };
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting process A\n");
    while (1) { // 1 aka true, if ur as dumb as me
        putchar('A');
        yield();
        // switch_context(&proc_a->sp, &proc_b->sp);
        // delay();
    }
}

void proc_b_entry(void) {
    printf("starting process B\n");
    while (1) {
        putchar('B');
        yield();
        // switch_context(&proc_b->sp, &proc_a->sp);
        // delay();
    }
}

// let's begin with the scheduler (dumbcoder youtube channel includes good videos about it, helping you imagining it) 
struct process *current_proc; // currently running the process
struct process *idle_proc; // Idle process
// what does idle process means ? 
// it's the process to run when there are no runnable processes



void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);
    
    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);
    
    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
    if((table1[vpn1] & PAGE_V) == 0) { // hello man, is first level page exists or not ?
        // ain't existing ?!! oky, let's create it
        uint32_t pt_paddr = alloc_pages(1);
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }
    // it should be created now!!, se let's move on with the 2nd level
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}



void kernel_main(void) {
    // const char *s = "\n\nHello World!\n";
    // for (int i = 0; s[i] != '\0'; ++i) {
    //     putchar(s[i]);
    // }
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    disk = (struct block_device *) alloc_pages(1);
    disk->read = ramdisk_read;
    disk->write = ramdisk_write;
    disk->block_count = 1024;

    // Initialize file system
    inodes[0].size = 0;
    memset(inodes[0].blocks, 0, sizeof(inodes[0].blocks));
    inodes[0].blocks[0] = 10; // allocate first data block

    // paddr_t paddr0 = alloc_pages(2);
    // paddr_t paddr1 = alloc_pages(1);
    // printf("alloc_pages test: paddr0=%x\n", paddr0);
    // printf("alloc_pages test: paddr1=%x\n", paddr1);
    // PANIC("booted!");

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    // idle_proc = create_process((uint32_t) NULL);
    idle_proc = create_process(NULL, 0);
    idle_proc->pid = 0; // idle wink wink
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    yield();
    PANIC("switched to idle process");
    // proc_a_entry();
    /* those are commented as we're using the scheduler now */
    // PANIC("unreachable here!");

    printf("\n\nHello %s\n", "World!");
    printf("1 + 2 = %d, %x\n", 1 + 2, 0x1234abcd);

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}

long sbi_console_putchar(int ch);


