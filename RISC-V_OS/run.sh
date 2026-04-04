#!/bin/bash
set -xue

# QEMU file path
QEMU=qemu-system-riscv32

#yalla ya qemu
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot