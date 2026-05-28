# Joe'sOS Roadmap

This roadmap uses SerenityOS as a reference point, but keeps Joe'sOS intentionally small and achievable.

## Current State

Joe'sOS already has:

- a boot sector
- a 32-bit freestanding kernel
- basic interrupt handling
- timer ticks and keyboard input
- a tiny command loop
- a minimal process table and round-robin bookkeeping
- a flat build image for QEMU

That is a solid base. The next step is to turn it into a layered system instead of a single growing kernel file.

## Design Goals

- keep boot code separate from kernel code
- split hardware-specific logic from shell and UI logic
- introduce clear driver boundaries
- make the build produce predictable artifacts
- add documentation alongside each subsystem

## Phase 1: Clean Kernel Boundaries

- move low-level helpers into dedicated files
- split console, serial, timer, and keyboard code
- keep the interrupt table and PIC/PIT setup isolated
- define a small kernel API for printing, input, and timing

## Phase 2: Core Subsystems

- add a heap allocator
- add physical and virtual memory management
- add paging setup and page fault handling
- create a basic task structure for future multitasking

## Phase 3: Userland Shape

- add a small init process or command launcher
- move shell commands into separate modules
- keep kernel-only code away from command logic
- add simple system-call style interfaces instead of direct kernel coupling

## Phase 4: Driver Model

- define a device abstraction
- move keyboard and timer behind drivers
- add a serial console backend
- prepare for disk and filesystem drivers

## Phase 5: Filesystem and Processes

- add a RAM-backed filesystem first
- then add a minimal persistent filesystem
- expand processes into separate address spaces, file descriptors, and exec-style program loading
- keep the kernel-user boundary explicit

## What To Copy From Serenity

- folder structure by subsystem, not by file type
- documentation for every major component
- build scripts that expose common tasks like build, run, clean
- a gradual expansion strategy instead of a big rewrite
- strict separation between kernel, libraries, services, and tools

## Suggested Order Of Work

1. split Joe'sOS into `boot/`, `kernel/`, `drivers/`, `userland/`, and `docs/`
2. extract the kernel helpers into smaller translation units
3. add memory management
4. add a process model
5. add a tiny filesystem and loader

## Non-Goals For Now

- GUI
- networking
- complex packages or ports
- large-scale portability

Those can come later once the kernel and userland are stable.
