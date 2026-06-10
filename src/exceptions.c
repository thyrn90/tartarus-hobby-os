#include "exceptions.h"
#include "display.h"
#include "api_manifold.h"

void GPFaultHandler (INT_REGISTERS *regs) {
    ClearScreen();

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0,  "===================================================", 0x00FF0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 8,  "    HARDWARE EXCEPTION: GENERAL PROTECTION FAULT   ", 0x00FF0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 16, "===================================================", 0x00FF0000);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 32, "Hardware Error Code          : ", 0x00FFFFFF);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, 250, 32, regs->error_code, 0x0000FFFF);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 40, "Instruction Pointer     (RIP): ", 0x00FFFFFF);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, 250, 40, regs->rip, 0x0000FFFF);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 48, "[!] CAUSE: Invalid MSR access, Privilege Violation, or Bad Segment.", 0x00AAAAAA);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 104, "SYSTEM HALTED. HARDWARE REBOOT IN 5 SECONDS...", 0x00FF0000);

    for (volatile long long i = 0; i < 900000000; i++) {}

    __asm__ __volatile__(
        "mov $0xFE, %%al\n\t"
        "outb %%al, $0x64\n\t"
        : : : "al"
    );

    while(1) { __asm__ __volatile__("cli; hlt"); } 
}
void PageFaultHandler(INT_REGISTERS *regs) {
    UINT64 cr2_val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2_val));

    if (cr2_val >= 0xFFFFFF0000000000ULL) {
        
        ClearScreen();
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0,  "===================================================", 0x00FF00FF);
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 8,  "    [TARTARUS] ILLUSION ENGINE: ANALYSIS PAUSE     ", 0x00FF00FF);
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 16, "===================================================", 0x00FF00FF);
        
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 32, "Target Address (CR2) : ", 0x00FFFFFF);
        DrawHex64(GlobalFrameBuffer, GlobalPitch, 200, 32, cr2_val, 0x0000FFFF);
        
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 40, "Faulting RIP Address : ", 0x00FFFFFF);
        DrawHex64(GlobalFrameBuffer, GlobalPitch, 200, 40, regs->rip, 0x0000FFFF);
        
        UINT64 opcode_chunk1 = *(UINT64*)(regs->rip);
        UINT64 opcode_chunk2 = *(UINT64*)(regs->rip + 8);
        
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 48, "Raw Opcodes (Part 1) : ", 0x00FFAA00);
        DrawHex64(GlobalFrameBuffer, GlobalPitch, 200, 48, opcode_chunk1, 0x00FFAA00);
        
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 56, "Raw Opcodes (Part 2) : ", 0x00FFAA00);
        DrawHex64(GlobalFrameBuffer, GlobalPitch, 200, 56, opcode_chunk2, 0x00FFAA00);

        UINT64 flight_recorder = *(UINT64*)(regs->rsp);
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 64, "Flight Recorder [RSP]: ", 0x00FF00FF);
        DrawHex64(GlobalFrameBuffer, GlobalPitch, 200, 64, flight_recorder, 0x00FFFF00);

        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 88, "Last API Targeted    : ", 0x00FFAA00);
        DrawString(GlobalFrameBuffer, GlobalPitch, 200, 88, GlobalLastAPI, 0x00FFFFFF);
        
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 72, "[!] Halting system to prevent Page Fault Storm.", 0x00FF0000);
        DrawString(GlobalFrameBuffer, GlobalPitch, 0, 80, "Check Opcodes (Little-Endian) to find instruction length.", 0x00AAAAAA);

        while(1) {
            __asm__ __volatile__("cli; hlt");
        }
    }

    if (regs->rip < 0x1000) {
        UINT64 return_address = *(UINT64*)(regs->rsp);
        regs->rip = return_address;
        regs->rsp += 8;
        regs->rax = 0;
        return; 
    }

    if (cr2_val < 0x1000) {
        UINT64 safe_zone = 0x0000000006000000ULL;
        
        if (regs->rax == 0) regs->rax = safe_zone;
        if (regs->rbx == 0) regs->rbx = safe_zone;
        if (regs->rcx == 0) regs->rcx = safe_zone;
        if (regs->rdx == 0) regs->rdx = safe_zone;
        if (regs->rsi == 0) regs->rsi = safe_zone;
        if (regs->rdi == 0) regs->rdi = safe_zone;
        if (regs->rbp == 0) regs->rbp = safe_zone;
        
        return;
    }

    ClearScreen();
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0,  "===================================================", 0x00FF0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 8,  "    CRITICAL KERNEL PANIC: UNHANDLED EXCEPTION     ", 0x00FF0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 16, "===================================================", 0x00FF0000);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 32, "Faulting Memory Address (CR2): ", 0x00FFFFFF);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, 250, 32, cr2_val, 0x0000FFFF);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 40, "Instruction Pointer     (RIP): ", 0x00FFFFFF);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, 250, 40, regs->rip, 0x0000FFFF);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 48, "Hardware Error Code          : ", 0x00FFFFFF);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, 250, 48, regs->error_code, 0x0000FFFF);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 64, "RAX:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 40, 64, regs->rax, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 72, "RBX:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 40, 72, regs->rbx, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 80, "RCX:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 40, 80, regs->rcx, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 88, "RDX:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 40, 88, regs->rdx, 0x00AA0000);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 220, 64, "RSI:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 260, 64, regs->rsi, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 220, 72, "RDI:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 260, 72, regs->rdi, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 220, 80, "RBP:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 260, 80, regs->rbp, 0x00AA0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 220, 88, "RSP:", 0x00AAAAAA); DrawHex64(GlobalFrameBuffer, GlobalPitch, 260, 88, regs->rsp, 0x00AA0000);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 104, "SYSTEM HALTED. REBOOT REQUIRED.", 0x00FF0000);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 104, "SYSTEM HALTED. HARDWARE REBOOT IN 5 SECONDS...", 0x00FF0000);

    for (volatile long long i = 0; i < 900000000; i++) {}

    __asm__ __volatile__(
        "mov $0xFE, %%al\n\t"
        "outb %%al, $0x64\n\t"
        : : : "al"
    );

    while(1) {
        __asm__ __volatile__("cli; hlt");
    }
}