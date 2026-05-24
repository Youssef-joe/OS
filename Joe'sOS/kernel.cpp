using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

namespace {
constexpr u16 kVgaWidth = 80;
constexpr u16 kVgaHeight = 25;
constexpr u16 kDefaultColor = 0x0F;
constexpr u16 kCommandBufferSize = 128;
constexpr u16 kSerialPort = 0x3F8;

volatile u16* const kVideoMemory = reinterpret_cast<u16*>(0xB8000);
u16 g_row = 0;
u16 g_column = 0;
u16 g_color = kDefaultColor;

inline void outb(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline u8 inb(u16 port) {
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

u16 make_vga_entry(char character, u16 color) {
    return static_cast<u16>(character) | static_cast<u16>(color << 8);
}

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

void print(const char* text) {
    for (u32 i = 0; text[i] != '\0'; ++i) {
        put_char(text[i]);
    }
}

void clear_screen() {
    g_row = 0;
    g_column = 0;
    for (u16 row = 0; row < kVgaHeight; ++row) {
        clear_row(row);
    }
}

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

bool starts_with(const char* text, const char* prefix) {
    for (u32 i = 0; prefix[i] != '\0'; ++i) {
        if (text[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

const char* scancode_to_char(u8 scancode) {
    switch (scancode) {
        case 0x02: return "1";
        case 0x03: return "2";
        case 0x04: return "3";
        case 0x05: return "4";
        case 0x06: return "5";
        case 0x07: return "6";
        case 0x08: return "7";
        case 0x09: return "8";
        case 0x0A: return "9";
        case 0x0B: return "0";
        case 0x10: return "q";
        case 0x11: return "w";
        case 0x12: return "e";
        case 0x13: return "r";
        case 0x14: return "t";
        case 0x15: return "y";
        case 0x16: return "u";
        case 0x17: return "i";
        case 0x18: return "o";
        case 0x19: return "p";
        case 0x1E: return "a";
        case 0x1F: return "s";
        case 0x20: return "d";
        case 0x21: return "f";
        case 0x22: return "g";
        case 0x23: return "h";
        case 0x24: return "j";
        case 0x25: return "k";
        case 0x26: return "l";
        case 0x2C: return "z";
        case 0x2D: return "x";
        case 0x2E: return "c";
        case 0x2F: return "v";
        case 0x30: return "b";
        case 0x31: return "n";
        case 0x32: return "m";
        case 0x39: return " ";
        case 0x0C: return "-";
        case 0x0D: return "=";
        default: return nullptr;
    }
}

char read_key() {
    for (;;) {
        if ((inb(0x64) & 0x01) == 0) {
            continue;
        }

        u8 scancode = inb(0x60);
        if (scancode & 0x80) {
            continue;
        }

        if (scancode == 0x1C) {
            return '\n';
        }

        if (scancode == 0x0E) {
            return '\b';
        }

        const char* mapped = scancode_to_char(scancode);
        if (mapped != nullptr) {
            return mapped[0];
        }
    }
}

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

void halt_forever() {
    print("System halted.\n");
    for (;;) {
        asm volatile("cli; hlt");
    }
}

void show_help() {
    print("help  - list commands\n");
    print("about - show OS info\n");
    print("clear - clear the screen\n");
    print("echo  - print text back\n");
    print("halt  - stop the CPU\n");
}

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

    if (strings_equal(command, "halt")) {
        halt_forever();
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

void prompt() {
    print("joe@os> ");
}
}  // namespace

extern "C" void kernel_main() {
    serial_init();
    clear_screen();

    print("Joe's OS\n");
    print("Type 'help' to get started.\n\n");

    char command[kCommandBufferSize];

    for (;;) {
        prompt();

        u16 length = 0;
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
