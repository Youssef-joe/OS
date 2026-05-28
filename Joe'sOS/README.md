# Joe's OS

This project builds a tiny 32-bit operating system kernel in C++ and packages it into a bootable floppy image that runs in QEMU on macOS.
It now includes a basic interrupt setup, PIT timer ticks, IRQ-driven keyboard input, and a small in-kernel process table.

## Requirements

- `clang`
- `clang++`
- `ld.lld`
- `llvm-objcopy`
- `nasm`
- `qemu-system-i386`

## Commands

- `make` builds `build/joesos.img`
- `make run` launches the OS with a VGA window
- `make run-headless` launches it with serial output in the terminal
- `make clean` removes build artifacts

## Available shell commands inside the OS

- `help`
- `about`
- `clear`
- `echo <text>`
- `ps`
- `proc`
- `spawn <name>`
- `kill <pid>`
- `halt`
- `uptime`

## Scaling Joe'sOS

SerenityOS is a good reference because it separates the system into clear layers:

- boot and early CPU setup
- kernel core and interrupt handling
- drivers and hardware abstractions
- userland tools and services
- build/run scripts and documentation

Joe'sOS is currently much smaller, but it can grow the same way if we keep those boundaries explicit.
See [`ROADMAP.md`](ROADMAP.md) for a practical phased path forward.
