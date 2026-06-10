#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <efi.h>

typedef struct {
    UINT64 r15, r14, r13, r12, r11, r10, r9, r8;
    UINT64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    UINT64 error_code; 
    
    UINT64 rip, cs, rflags, rsp, ss;
} __attribute__((packed)) INT_REGISTERS;

void PageFaultHandler(INT_REGISTERS *regs);

#endif