# Tartarus

**CRITICAL WARNING: DO NOT RUN ON PHYSICAL HARDWARE (BARE METAL)**
*Note: This project is currently under active development.*

This is an experimental, Ring-0 hobby operating system built from scratch for low-level hardware research and reverse engineering. It directly manipulates physical memory, PCI buses, and CPU registers without any safety nets. Running this on physical hardware may cause severe system instability, permanent data loss, or hardware damage. 
**Run exclusively on QEMU or similar virtual machines.**

## The Concept

Tartarus is not a consumer operating system or a commercial sandbox. It is a custom-built, hypervisor-less kernel operating entirely in Ring-0. By stripping away modern OS abstractions, it provides a bare-metal environment to execute, manipulate, and analyze complex executables (PE / Raw Binary) directly on the CPU.

## Core Architecture

Written entirely in C and x86_64 Assembly. After the initial boot sequence, the kernel manages its own hardware interfaces without relying on external libraries or UEFI runtime services.

* **Interrupt Handling:** Custom assembly wrappers for hardware exceptions (#GP, #PF, #DB) with a minimalist GDT/IDT setup.
* **Memory Management:** Parses the UEFI Memory Map and uses a custom dynamic heap allocator supporting block splitting and coalescing.
* **Hardware Enumeration:** Direct PCI/PCIe bus scanning and raw ATA drive sector reading.
* **Native Filesystem:** Built-in FAT32 parser to extract and execute binaries directly from the disk.

## Subsystems

Tartarus includes specific mechanisms designed to manipulate externally loaded programs within a Ring-0 vacuum.

### Dynamic PE Execution Engine
Tartarus does not rely on static entry points. It dynamically maps PE sections into virtual memory, scans for MSVC and GCC (MinGW-w64) compiler signatures, and bypasses standard CRT boilerplate code to drop execution exactly at the original `main()` function.

### Page Fault Interception & IAT Hooking
Target executables often query missing APIs or attempt to write to restricted memory. Instead of crashing, Tartarus intercepts Page Faults (#PF) and hooks the Import Address Table (IAT).
* **Custom PEB/LDR Chain:** Constructs a circular, doubly-linked fake PEB/LDR chain to inject synthetic `ntdll.dll` and `KERNEL32.DLL` modules.
* **Dynamic Trampolines:** Compiles x86_64 assembly trampolines on the fly for intercepted API calls.
* **Amnesia Stub:** A dedicated physical memory region (0x06000000) filled with `xor rax, rax; ret` instructions. When the target executable attempts to access unauthorized or missing functions, Tartarus redirects the Instruction Pointer (RIP) here, feeding the program amnesiac data to keep the execution flow alive.

### Hardware-Level Debugger
Utilizes x86_64 Debug Registers (DR0-DR7) and a custom Vector 1 (ISR1) handler for Trap Flag (single-step) execution. It includes a built-in trap mechanism to catch hardware watchpoints and dump real-time CPU register states (RAX, RBX, RCX, etc.) directly to the framebuffer.

## Building from Source

**Compiler Warning:** You must use GCC (MinGW-w64 on Windows or standard GCC on Linux). Do NOT use MSVC. Tartarus relies heavily on GCC-specific GNU inline assembly (`__asm__ volatile`) and `__attribute__((naked))` directives for Ring-0 state manipulation, which MSVC does not support for x64 targets.

Requires a cross-compiler toolchain and `gnu-efi`.

```
# 1. Compile core ASM interrupt wrappers
nasm -f win64 src/isr.asm -o src/isr.o
nasm -f win64 src/trampoline.asm -o src/trampoline.o
```

# 2. Build the Tartarus EFI binary
make

## Running in QEMU
**Tartarus requires a FAT32 formatted disk image to boot via UEFI and to parse payloads locally using its native fat32.c driver.**

* Create a 50MB FAT32 disk image (disk.img).

* Copy the compiled BOOTX64.efi into the EFI/BOOT/ directory inside the disk image.

* Place your target payloads (e.g., subject.exe or raw shellcode) directly into the root directory of the disk image. Tartarus's autopsy engine will automatically scan and map the first valid executable it finds.

**Run via QEMU with OVMF firmware:**

```
qemu-system-x86_64 -bios OVMF.fd -drive file=disk.img,format=raw -m 512M -vga std
```

**Architect: thyrn90**