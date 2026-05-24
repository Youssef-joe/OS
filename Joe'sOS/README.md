# Joe's OS

This project builds a tiny 32-bit operating system kernel in C++ and packages it into a bootable floppy image that runs in QEMU on macOS.

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
- `halt`
