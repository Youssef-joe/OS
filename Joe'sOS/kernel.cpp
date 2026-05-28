using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

extern "C" void load_idt(const void* idt_descriptor);
extern "C" void enable_interrupts();
extern "C" void disable_interrupts();
extern "C" void* isr_stub_table[];

namespace {
// Tiny kernel constants: the numbers that make hardware stop being mysterious and start being bossed around.
constexpr u16 kVgaWidth = 80;
constexpr u16 kVgaHeight = 25;
constexpr u16 kDefaultColor = 0x0F;
constexpr u16 kCommandBufferSize = 128;
constexpr u16 kKeyboardBufferSize = 256;
constexpr u16 kSerialPort = 0x3F8;
constexpr u8 kPic1Command = 0x20;
constexpr u8 kPic1Data = 0x21;
constexpr u8 kPic2Command = 0xA0;
constexpr u8 kPic2Data = 0xA1;
constexpr u8 kPitCommand = 0x43;
constexpr u8 kPitChannel0 = 0x40;
constexpr u32 kPitFrequency = 100;

// Global state: the kernel's short-term memory, which is exactly as elegant as it sounds.
volatile u16* const kVideoMemory = reinterpret_cast<u16*>(0xB8000);
u16 g_row = 0;
u16 g_column = 0;
u16 g_color = kDefaultColor;
volatile char g_keyboard_buffer[kKeyboardBufferSize];
volatile u16 g_keyboard_head = 0;
volatile u16 g_keyboard_tail = 0;
volatile u64 g_timer_ticks = 0;

enum class ProcessState : u8 {
    Unused,
    Ready,
    Running,
    Exited,
};

struct Process {
    u32 pid;
    char name[16];
    ProcessState state;
    u32 cpu_ticks;
    u32 start_tick;
};

constexpr u32 kMaxProcesses = 8;
constexpr u32 kProcessTimeSliceTicks = 5;
constexpr u32 kProcessNameSize = 16;

Process g_processes[kMaxProcesses];
u32 g_process_count = 0;
u32 g_next_pid = 1;
u32 g_current_process_index = 0;
u32 g_process_slice_ticks = 0;
bool g_processes_initialized = false;

struct [[gnu::packed]] IdtEntry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
};

struct [[gnu::packed]] IdtDescriptor {
    u16 size;
    u32 offset;
};

struct InterruptFrame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 interrupt_number;
    u32 error_code;
};

IdtEntry g_idt[256];
IdtDescriptor g_idt_descriptor;

// Port I/O: the old-fashioned way to talk to hardware, because sometimes the hardware refuses email.
inline void outb(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline u8 inb(u16 port) {
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// VGA helpers: because text-mode graphics are what happens when ambition meets 1980s furniture.
u16 make_vga_entry(char character, u16 color) {
    return static_cast<u16>(character) | static_cast<u16>(color << 8);
}

// Serial output: for when the screen is too glamorous and we need the terminal to suffer too.
void serial_write_char(char c) {
    while ((inb(kSerialPort + 5) & 0x20) == 0) {
    }

    outb(kSerialPort, static_cast<u8>(c));
}

void serial_init() {
    outb(kSerialPort + 1, 0x00);
    outb(kSerialPort + 3, 0x80);
    outb(kSerialPort + 0, 0x03);
    outb(kSerialPort + 1, 0x00);
    outb(kSerialPort + 3, 0x03);
    outb(kSerialPort + 2, 0xC7);
    outb(kSerialPort + 4, 0x0B);
}

// Screen management: the part where the kernel acts like a tiny stage manager.
void clear_row(u16 row) {
    for (u16 col = 0; col < kVgaWidth; ++col) {
        kVideoMemory[row * kVgaWidth + col] = make_vga_entry(' ', g_color);
    }
}

void scroll_if_needed() {
    if (g_row < kVgaHeight) {
        return;
    }

    for (u16 row = 1; row < kVgaHeight; ++row) {
        for (u16 col = 0; col < kVgaWidth; ++col) {
            kVideoMemory[(row - 1) * kVgaWidth + col] =
                kVideoMemory[row * kVgaWidth + col];
        }
    }

    clear_row(kVgaHeight - 1);
    g_row = kVgaHeight - 1;
}

// Character output: one glyph at a time, like a printer with opinions.
void put_char(char c) {
    if (c == '\n') {
        g_column = 0;
        ++g_row;
        scroll_if_needed();
        serial_write_char('\r');
        serial_write_char('\n');
        return;
    }

    if (c == '\r') {
        g_column = 0;
        serial_write_char('\r');
        return;
    }

    kVideoMemory[g_row * kVgaWidth + g_column] = make_vga_entry(c, g_color);
    serial_write_char(c);
    ++g_column;

    if (g_column >= kVgaWidth) {
        g_column = 0;
        ++g_row;
        scroll_if_needed();
        serial_write_char('\r');
        serial_write_char('\n');
    }
}

// String helpers: because std::string is not invited to this freestanding party.
void print(const char* text) {
    for (u32 i = 0; text[i] != '\0'; ++i) {
        put_char(text[i]);
    }
}

void print_u32(u32 value) {
    if (value == 0) {
        put_char('0');
        return;
    }

    char digits[10];
    u32 length = 0;
    while (value > 0) {
        digits[length++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (length > 0) {
        put_char(digits[--length]);
    }
}

void print_process_state(ProcessState state) {
    switch (state) {
        case ProcessState::Unused:
            print("unused");
            return;
        case ProcessState::Ready:
            print("ready");
            return;
        case ProcessState::Running:
            print("running");
            return;
        case ProcessState::Exited:
            print("exited");
            return;
    }
}

Process* find_process_by_pid(u32 pid) {
    for (u32 i = 0; i < kMaxProcesses; ++i) {
        if (g_processes[i].state != ProcessState::Unused && g_processes[i].pid == pid) {
            return &g_processes[i];
        }
    }

    return nullptr;
}

void copy_process_name(char* destination, const char* source);

Process* create_process(const char* name) {
    for (u32 i = 0; i < kMaxProcesses; ++i) {
        if (g_processes[i].state == ProcessState::Unused) {
            Process& process = g_processes[i];
            process.pid = g_next_pid++;
            copy_process_name(process.name, name);
            process.state = ProcessState::Ready;
            process.cpu_ticks = 0;
            process.start_tick = static_cast<u32>(g_timer_ticks);
            ++g_process_count;
            return &process;
        }
    }

    return nullptr;
}

void initialize_processes() {
    if (g_processes_initialized) {
        return;
    }

    for (u32 i = 0; i < kMaxProcesses; ++i) {
        g_processes[i].pid = 0;
        for (u32 j = 0; j < kProcessNameSize; ++j) {
            g_processes[i].name[j] = '\0';
        }
        g_processes[i].state = ProcessState::Unused;
        g_processes[i].cpu_ticks = 0;
        g_processes[i].start_tick = 0;
    }

    Process* idle = create_process("idle");
    Process* shell = create_process("shell");
    Process* clock = create_process("clock");

    if (idle != nullptr) {
        idle->state = ProcessState::Running;
        g_current_process_index = static_cast<u32>(idle - g_processes);
    }

    if (shell != nullptr) {
        shell->state = ProcessState::Ready;
    }

    if (clock != nullptr) {
        clock->state = ProcessState::Ready;
    }

    g_processes_initialized = true;
}

void schedule_processes() {
    if (g_process_count == 0) {
        return;
    }

    ++g_process_slice_ticks;
    Process& current = g_processes[g_current_process_index];
    if (current.state != ProcessState::Unused && current.state != ProcessState::Exited) {
        ++current.cpu_ticks;
    }

    if (g_process_slice_ticks < kProcessTimeSliceTicks) {
        return;
    }

    g_process_slice_ticks = 0;

    if (current.state == ProcessState::Running) {
        current.state = ProcessState::Ready;
    }

    for (u32 offset = 1; offset <= kMaxProcesses; ++offset) {
        const u32 candidate_index = static_cast<u32>((g_current_process_index + offset) % kMaxProcesses);
        Process& candidate = g_processes[candidate_index];
        if (candidate.state == ProcessState::Ready) {
            candidate.state = ProcessState::Running;
            g_current_process_index = candidate_index;
            return;
        }
    }

    if (current.state != ProcessState::Unused) {
        current.state = ProcessState::Running;
    }
}

void print_process_table() {
    print("PID  STATE    CPU  NAME\n");
    for (u32 i = 0; i < kMaxProcesses; ++i) {
        const Process& process = g_processes[i];
        if (process.state == ProcessState::Unused) {
            continue;
        }

        print_u32(process.pid);
        print("   ");
        print_process_state(process.state);
        print("   ");
        print_u32(process.cpu_ticks);
        print("   ");
        print(process.name);
        print("\n");
    }
}

Process* current_process() {
    if (g_process_count == 0) {
        return nullptr;
    }

    if (g_processes[g_current_process_index].state == ProcessState::Unused) {
        for (u32 i = 0; i < kMaxProcesses; ++i) {
            if (g_processes[i].state != ProcessState::Unused) {
                g_current_process_index = i;
                break;
            }
        }
    }

    return &g_processes[g_current_process_index];
}

// Console housekeeping: reset the stage before the next round of tiny command theater.
void clear_screen() {
    g_row = 0;
    g_column = 0;
    for (u16 row = 0; row < kVgaHeight; ++row) {
        clear_row(row);
    }
}

// Plain string utilities: no cleverness, just enough honesty to compare bytes.
bool strings_equal(const char* left, const char* right) {
    u32 index = 0;
    while (left[index] != '\0' || right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        ++index;
    }
    return true;
}

// Prefix matching: the kernel's version of "close enough, continue talking."
bool starts_with(const char* text, const char* prefix) {
    for (u32 i = 0; prefix[i] != '\0'; ++i) {
        if (text[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

// Keyboard map: translated from scancodes into human-friendly regret.
char scancode_to_char(u8 scancode) {
    switch (scancode) {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x39: return ' ';
        case 0x0C: return '-';
        case 0x0D: return '=';
        default: return '\0';
    }
}

// Ring buffer: because keyboard input arrives whenever it feels like it.
void enqueue_key(char key) {
    const u16 next = static_cast<u16>((g_keyboard_head + 1) % kKeyboardBufferSize);
    if (next == g_keyboard_tail) {
        return;
    }

    g_keyboard_buffer[g_keyboard_head] = key;
    g_keyboard_head = next;
}

// IDT setup: teaching the CPU where to send its complaints.
void set_idt_gate(u8 vector, void (*handler)()) {
    const u32 address = reinterpret_cast<u32>(handler);
    g_idt[vector].offset_low = static_cast<u16>(address & 0xFFFF);
    g_idt[vector].selector = 0x08;
    g_idt[vector].zero = 0;
    g_idt[vector].type_attributes = 0x8E;
    g_idt[vector].offset_high = static_cast<u16>((address >> 16) & 0xFFFF);
}

// Interrupt plumbing: 48 real stubs, then a long list of "please don't do anything weird."
void initialize_idt() {
    for (u32 vector = 0; vector < 48; ++vector) {
        set_idt_gate(static_cast<u8>(vector),
                     reinterpret_cast<void (*)()>(isr_stub_table[vector]));
    }

    for (u32 vector = 48; vector < 256; ++vector) {
        set_idt_gate(static_cast<u8>(vector),
                     reinterpret_cast<void (*)()>(isr_stub_table[0]));
    }

    g_idt_descriptor.size = static_cast<u16>(sizeof(g_idt) - 1);
    g_idt_descriptor.offset = reinterpret_cast<u32>(&g_idt[0]);
    load_idt(&g_idt_descriptor);
}

// PIC remapping: moving the interrupt furniture so the CPU stops tripping over it.
void remap_pic() {
    const u8 master_mask = inb(kPic1Data);
    const u8 slave_mask = inb(kPic2Data);

    outb(kPic1Command, 0x11);
    outb(kPic2Command, 0x11);

    outb(kPic1Data, 0x20);
    outb(kPic2Data, 0x28);

    outb(kPic1Data, 0x04);
    outb(kPic2Data, 0x02);

    outb(kPic1Data, 0x01);
    outb(kPic2Data, 0x01);

    outb(kPic1Data, master_mask);
    outb(kPic2Data, slave_mask);
}

// IRQ masks: the kernel's very selective guest list.
void set_irq_mask(u8 irq, bool masked) {
    const u16 port = irq < 8 ? kPic1Data : kPic2Data;
    const u8 bit = irq < 8 ? irq : static_cast<u8>(irq - 8);
    u8 value = inb(port);

    if (masked) {
        value = static_cast<u8>(value | (1u << bit));
    } else {
        value = static_cast<u8>(value & ~(1u << bit));
    }

    outb(port, value);
}

// PIT setup: the clock gets a personality, and then starts interrupting us on schedule.
void initialize_timer(u32 frequency) {
    const u32 divisor = 1193180 / frequency;
    outb(kPitCommand, 0x36);
    outb(kPitChannel0, static_cast<u8>(divisor & 0xFF));
    outb(kPitChannel0, static_cast<u8>((divisor >> 8) & 0xFF));
}

// End-of-interrupt: the polite little wave that says "thanks, we saw that."
void send_eoi(u8 interrupt_number) {
    if (interrupt_number >= 40) {
        outb(kPic2Command, 0x20);
    }
    outb(kPic1Command, 0x20);
}

// Busy-waiting with a nap button: not glamorous, but very on-brand for early kernels.
char read_key() {
    for (;;) {
        disable_interrupts();
        if (g_keyboard_head != g_keyboard_tail) {
            const char key = g_keyboard_buffer[g_keyboard_tail];
            g_keyboard_tail = static_cast<u16>((g_keyboard_tail + 1) % kKeyboardBufferSize);
            enable_interrupts();
            return key;
        }

        enable_interrupts();
        asm volatile("hlt");
    }
}

// Backspace: undoing bad life choices one column at a time.
void backspace() {
    if (g_column == 0) {
        return;
    }

    --g_column;
    kVideoMemory[g_row * kVgaWidth + g_column] = make_vga_entry(' ', g_color);
    serial_write_char('\b');
    serial_write_char(' ');
    serial_write_char('\b');
}

// Halt forever: the kernel's version of "I'm done, you may all leave now."
void halt_forever() {
    print("System halted.\n");
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// Built-in help: because even tiny operating systems deserve a manual, sort of.
void show_help() {
    print("help  - list commands\n");
    print("about - show OS info\n");
    print("clear - clear the screen\n");
    print("echo  - print text back\n");
    print("ps    - show process table\n");
    print("proc  - show current process\n");
    print("spawn - create a demo process\n");
    print("kill  - remove a process by pid\n");
    print("halt  - stop the CPU\n");
    print("uptime - show timer ticks\n");
}

bool parse_u32(const char* text, u32& value) {
    value = 0;
    if (text[0] == '\0') {
        return false;
    }

    for (u32 i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }

        value = static_cast<u32>(value * 10 + static_cast<u32>(text[i] - '0'));
    }

    return true;
}

void copy_process_name(char* destination, const char* source) {
    u32 index = 0;
    for (; index + 1 < kProcessNameSize; ++index) {
        if (source[index] == '\0') {
            break;
        }

        destination[index] = source[index];
    }

    destination[index] = '\0';
}

// Command dispatcher: the tiny bureaucracy that decides what the shell is allowed to do.
void handle_command(const char* command) {
    if (strings_equal(command, "help")) {
        show_help();
        return;
    }

    if (strings_equal(command, "about")) {
        print("Joe's OS is a tiny 32-bit C++ kernel with VGA, serial, and keyboard support.\n");
        return;
    }

    if (strings_equal(command, "clear")) {
        clear_screen();
        return;
    }

    if (strings_equal(command, "ps")) {
        print_process_table();
        return;
    }

    if (strings_equal(command, "proc")) {
        Process* process = current_process();
        if (process == nullptr) {
            print("No processes are running yet.\n");
            return;
        }

        print("Current process: ");
        print(process->name);
        print(" (pid ");
        print_u32(process->pid);
        print(", ");
        print_process_state(process->state);
        print(")\n");
        return;
    }

    if (starts_with(command, "spawn ")) {
        const char* name = command + 6;
        while (name[0] == ' ') {
            ++name;
        }

        if (name[0] == '\0') {
            print("Usage: spawn <name>\n");
            return;
        }

        disable_interrupts();
        Process* process = create_process(name);
        enable_interrupts();

        if (process == nullptr) {
            print("No free process slots.\n");
            return;
        }

        print("Spawned process ");
        print(process->name);
        print(" with pid ");
        print_u32(process->pid);
        print("\n");
        return;
    }

    if (starts_with(command, "kill ")) {
        const char* pid_text = command + 5;
        while (pid_text[0] == ' ') {
            ++pid_text;
        }

        u32 pid = 0;
        if (!parse_u32(pid_text, pid)) {
            print("Usage: kill <pid>\n");
            return;
        }

        disable_interrupts();
        Process* process = find_process_by_pid(pid);
        if (process != nullptr && process->pid != 1) {
            process->state = ProcessState::Unused;
            process->name[0] = '\0';
            process->pid = 0;
            process->cpu_ticks = 0;
            process->start_tick = 0;
            if (g_process_count > 0) {
                --g_process_count;
            }
        }
        enable_interrupts();

        if (process == nullptr) {
            print("No such process.\n");
            return;
        }

        if (process->pid == 1) {
            print("Refusing to kill idle.\n");
            return;
        }

        print("Killed pid ");
        print_u32(pid);
        print("\n");
        return;
    }

    if (strings_equal(command, "halt")) {
        halt_forever();
    }

    if (strings_equal(command, "uptime")) {
        const u32 ticks = static_cast<u32>(g_timer_ticks);
        print("Ticks: ");
        print_u32(ticks);
        print(" | Seconds: ");
        print_u32(ticks / kPitFrequency);
        print("\n");
        return;
    }

    if (starts_with(command, "echo ")) {
        print(command + 5);
        print("\n");
        return;
    }

    if (command[0] == '\0') {
        return;
    }

    print("Unknown command. Type 'help'.\n");
}

// Prompt: the kernel's way of saying "go ahead, impress me."
void prompt() {
    print("joe@os> ");
}
}  // namespace

// Interrupt handler: the place where hardware interrupts come in wearing tiny hats and demands.
extern "C" void interrupt_handler(InterruptFrame* frame) {
    // Timer IRQ: the one clock interrupt we invited on purpose.
    if (frame->interrupt_number == 32) {
        ++g_timer_ticks;
        schedule_processes();
        send_eoi(32);
        return;
    }

    // Keyboard IRQ: where scancodes arrive looking like cryptic goblins.
    if (frame->interrupt_number == 33) {
        const u8 scancode = inb(0x60);
        if ((scancode & 0x80) == 0) {
            if (scancode == 0x1C) {
                enqueue_key('\n');
            } else if (scancode == 0x0E) {
                enqueue_key('\b');
            } else {
                const char mapped = scancode_to_char(scancode);
                if (mapped != '\0') {
                    enqueue_key(mapped);
                }
            }
        }

        send_eoi(33);
        return;
    }

    if (frame->interrupt_number >= 32 && frame->interrupt_number <= 47) {
        send_eoi(static_cast<u8>(frame->interrupt_number));
    }
}

extern "C" void kernel_main() {
    // Startup sequence: wake up the serial port, clean the stage, and act like we planned this.
    serial_init();
    clear_screen();
    print("Joe's OS\n");
    print("Starting kernel services...\n");

    // Core kernel plumbing: interrupts, timers, and the minimum viable "things happening".
    initialize_idt();
    remap_pic();
    initialize_timer(kPitFrequency);
    set_irq_mask(0, false);
    set_irq_mask(1, false);
    initialize_processes();
    print("Interrupts online. Type 'help' to get started.\n\n");
    enable_interrupts();

    char command[kCommandBufferSize];

    // Tiny shell loop: the kernel's infinite customer service desk.
    for (;;) {
        prompt();

        u16 length = 0;
        // Read a command one key at a time, because drama is part of the experience.
        for (;;) {
            const char key = read_key();

            if (key == '\n') {
                command[length] = '\0';
                put_char('\n');
                break;
            }

            if (key == '\b') {
                if (length > 0) {
                    --length;
                    backspace();
                }
                continue;
            }

            if (length + 1 >= kCommandBufferSize) {
                continue;
            }

            command[length++] = key;
            put_char(key);
        }

        handle_command(command);
    }
}
