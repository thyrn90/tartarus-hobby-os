#include "idt.h"
#include "display.h"

struct IDTEntry idt[256];
struct IDTPointer idt_ptr;

void SetIDTGate (int num, uint64_t handler) {
    idt[num].offset_low = (uint16_t)(handler & 0xFFFF);
    idt[num].selector   = 0x08;
    idt[num].ist        = 0;
    idt[num].type_attr  = 0x8E;
    idt[num].offset_mid = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint16_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].zero       = 0;     
}

extern void outb(uint16_t port, uint8_t val); 

void Tartarus_HardwareFaultHandler(int error_code) {
    ClearScreen(); 
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 100, 100, "FATAL HARDWARE EXCEPTION DETECTED!", 0x00FF0000);
    DrawString(GlobalFrameBuffer, GlobalPitch, 100, 120, "TARTARUS KERNEL PANIC - BARE METAL PROTECTION ACTIVE", 0x00FF0000);
    
    if (error_code == 13) {
        DrawString(GlobalFrameBuffer, GlobalPitch, 100, 150, "EXCEPTION 0x0D: GENERAL PROTECTION FAULT (#GP)", 0x00FFFF00);
        DrawString(GlobalFrameBuffer, GlobalPitch, 100, 170, "[!] Did you read an invalid AMD/Intel MSR?", 0x00AAAAAA);
    } 
    else if (error_code == 14) {
        DrawString(GlobalFrameBuffer, GlobalPitch, 100, 150, "EXCEPTION 0x0E: PAGE FAULT (#PF)", 0x00FFFF00);
        DrawString(GlobalFrameBuffer, GlobalPitch, 100, 170, "[!] Invalid RAM access.", 0x00AAAAAA);
    }
    else {
        DrawString(GlobalFrameBuffer, GlobalPitch, 100, 150, "UNKNOWN CRITICAL FAULT.", 0x00FFFF00);
    }

    DrawString(GlobalFrameBuffer, GlobalPitch, 100, 220, "REBOOTING SYSTEM IN HARDWARE LEVEL...", 0x0000FF00);
    
    for (volatile int i = 0; i < 500000000; i++) {}

    outb(0x64, 0xFE); 

    while (1) {
        __asm__ __volatile__("cli; hlt");
    }
}

void isr13_handler() { Tartarus_HardwareFaultHandler(13); }
void isr14_handler() { Tartarus_HardwareFaultHandler(14); }

void Tartarus_InitIDT() {
    idt_ptr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    for(int i = 0; i < 256; i++) {
        SetIDTGate(i, 0); 
    }

    SetIDTGate(13, (uint64_t)isr13_handler); // #GP
    SetIDTGate(14, (uint64_t)isr14_handler); // #PF

    __asm__ __volatile__("lidt %0" : : "m"(idt_ptr));
}