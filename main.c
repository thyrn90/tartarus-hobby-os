/*
 * ==============================================================================
 * Project: Tartarus
 * Architect: thyrn90
 * License: BSD 2-Clause
 * Description:
 * A bare-metal, Ring-0 experimental operating system designed for low-level
 * hardware research and reverse engineering. It strips away modern OS abstractions,
 * providing a vacuum to execute and manipulate complex PE files directly on the CPU.
 *
 * Core Architecture & Subsystems:
 * - Interrupt Engine: Minimalist GDT/IDT with custom hardware exception wrappers.
 * - Memory Manager: Parses UEFI Memory Map with a custom dynamic heap allocator.
 * - Native FAT32 Parser: Extracts and executes binaries directly from the disk.
 * - Dynamic PE Engine: Maps sections, scans for MSVC/GCC signatures, bypasses 
 *   CRT stubs, and drops execution exactly at the original main() function.
 * - Page Fault Interceptor & IAT Hooking: Catches unmapped API calls.
 * - Custom LDR Chain: Injects synthetic ntdll.dll and KERNEL32.DLL modules.
 * - Dynamic Trampolines: Compiles x86_64 assembly on-the-fly for intercepts.
 * - Amnesia Stub (0x06000000): Redirects unauthorized or missing function calls 
 *   to a physical memory zone filled with 'xor rax, rax; ret' instructions, 
 *   feeding the executable amnesiac data to keep the execution flow alive.
 *
 * CRITICAL WARNING: DO NOT RUN ON BARE METAL. DESIGNED FOR QEMU ONLY.
 * ==============================================================================
 */

#include <efi.h>
#include "display.h"
#include "keyboard.h"
#include "mouse.h"
#include "shell.h"
#include "exceptions.h"
#include "memory.h"
#include "fat32.h"
#include "api_manifold.h"
#include "idt.h"

Tartarus_MemoryDescriptor* GlobalUefiMemoryMap = NULL;
UINT64 GlobalMMapSize = 0;
UINT64 GlobalDescriptorSize = 0;

#define HW_BP_TYPE_EXECUTE 0x00 // Trigger on instruction execution
#define HW_BP_TYPE_WRITE   0x01 // Trigger on memory write
#define HW_BP_TYPE_RW      0x03 // Trigger on memory read or write

// Hardware Breakpoint Lengths
#define HW_BP_LEN_1_BYTE   0x00
#define HW_BP_LEN_2_BYTES  0x01
#define HW_BP_LEN_4_BYTES  0x03
#define HW_BP_LEN_8_BYTES  0x02

// Low-level register access
static inline void write_dr0(UINT64 val) { __asm__ volatile("mov %0, %%dr0" :: "r"(val)); }
static inline void write_dr7(UINT64 val) { __asm__ volatile("mov %0, %%dr7" :: "r"(val)); }
static inline UINT64 read_dr6(void)      { UINT64 val; __asm__ volatile("mov %%dr6, %0" : "=r"(val)); return val; }

void ArmHardwareBreakpoint(UINT64 target_address, UINT8 type, UINT8 length) {
    write_dr0(target_address); 

    UINT64 dr7 = 0;
    
    dr7 |= 1; 

    dr7 |= ((UINT64)(type & 0x3) << 16);

    dr7 |= ((UINT64)(length & 0x3) << 18);

    write_dr7(dr7); 
}

#pragma pack(1)
typedef struct { UINT16 LimitLow; UINT16 BaseLow; UINT8 BaseMiddle; UINT8 Access; UINT8 FlagsLimitHigh; UINT8 BaseHigh; } GDT_ENTRY;
typedef struct { UINT16 Limit; UINTN Base; } POINTER_STRUCT;
typedef struct { UINT16 OffsetLow; UINT16 Selector; UINT8 Ist; UINT8 TypeAttr; UINT16 OffsetMid; UINT32 OffsetHigh; UINT32 Reserved; } IDT_ENTRY;
#pragma pack()

GDT_ENTRY GDT[3];
IDT_ENTRY IDT[256];
POINTER_STRUCT GDTR, IDTR;

void SetupGDT() {
    GDT[1] = (GDT_ENTRY){0xFFFF, 0, 0, 0x9A, 0xA0, 0}; 
    GDT[2] = (GDT_ENTRY){0xFFFF, 0, 0, 0x92, 0xA0, 0}; 
    GDTR.Limit = (sizeof(GDT_ENTRY) * 3) - 1; 
    GDTR.Base = (UINTN)&GDT;
    __asm__ __volatile__("lgdt %0" : : "m"(GDTR));
    __asm__ __volatile__("pushq $0x08; lea .reload_cs(%%rip), %%rax; pushq %%rax; retfq; .reload_cs: mov $0x10, %%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%gs; mov %%ax, %%ss;" ::: "rax");
}

void SetupPIC() {
    outb(0x20, 0x11); outb(0xA0, 0x11); 
    outb(0x21, 0x20); outb(0xA1, 0x28); 
    outb(0x21, 0x04); outb(0xA1, 0x02); 
    outb(0x21, 0x01); outb(0xA1, 0x01); 
    outb(0x21, 0xF9); outb(0xA1, 0xEF); 
}

void FatalExceptionHandler() {
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0, "FATAL EXCEPTION", 0x00FF0000);
    while(1) __asm__ __volatile__("cli; hlt");
}

extern void FatalWrapper(void); 
extern void KeyboardWrapper(void); 
extern void MouseWrapper(void); 
extern void IgnoreWrapper(void);
extern void PageFaultWrapper(void);
extern void ExecuteWithTrapFlag(void* payload_address);
extern void Isr1Handler();
extern void GPFaultWrapper();

__asm__ (
    ".global FatalWrapper\n\t FatalWrapper:\n\t cli\n\t pushq %rax\n\t call FatalExceptionHandler\n\t popq %rax\n\t sti\n\t iretq\n\t"
    ".global KeyboardWrapper\n\t KeyboardWrapper:\n\t cli\n\t pushq %rax\n\t call KeyboardHandler\n\t popq %rax\n\t sti\n\t iretq\n\t"
    ".global MouseWrapper\n\t MouseWrapper:\n\t cli\n\t pushq %rax\n\t call MouseHandler\n\t popq %rax\n\t sti\n\t iretq\n\t"
    ".global IgnoreWrapper\n\t IgnoreWrapper:\n\t iretq\n\t"
);

void SetIDTEntry(int index, void* wrapper) {
    UINTN addr = (UINTN)wrapper;
    IDT[index] = (IDT_ENTRY){(UINT16)(addr & 0xFFFF), 0x08, 0, 0x8E, (UINT16)((addr >> 16) & 0xFFFF), (UINT32)((addr >> 32) & 0xFFFFFFFF), 0};
}

typedef struct {
    UINT64 rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    
    UINT64 rip, cs, rflags, rsp, ss;
} __attribute__((packed)) CPU_STATE;

static inline UINT8 inb_debug(UINT16 port) {
    UINT8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void DrawByteHex(UINT8 byte, UINT32 x, UINT32 y, UINT32 color) {
    char str[3];
    const char* hexChars = "0123456789ABCDEF";
    
    str[0] = hexChars[(byte >> 4) & 0x0F];
    str[1] = hexChars[byte & 0x0F];
    str[2] = '\0'; 
    
    DrawString(GlobalFrameBuffer, GlobalPitch, x, y, str, color);
}

UINT64 Tartarus_ReadMSR(UINT32 msr_index) {
    UINT32 low, high;
    __asm__ __volatile__("rdmsr" : "=a"(low), "=d"(high) : "c"(msr_index));
    return((UINT64)high << 32) | low;

}

UINT8 Tartarus_InB(UINT16 port) {
    UINT8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void DebuggerEngine(CPU_STATE* state) {
    char hexBuf[19];

    UINT64 dr6_status = read_dr6();
    if (dr6_status & 1) { 
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] DR0 WATCHPOINT HIT! Halting execution...", 0x0000FF00);
        CursorY += 8;
        
        __asm__ volatile("mov %0, %%dr6" :: "r"(0ULL));
        
        state->rflags |= (1ULL << 8); 
    }

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[DEBUG] POLYMORPHIC TRAP - CPU STATE:", 0x00FF00FF);
    CursorY += 8;
    
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  RAX: ", 0x00AAAAAA);
    HexToString(state->rax, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 64, CursorY, hexBuf, 0x0000FF00);

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 300, CursorY, "RBX: ", 0x00AAAAAA); 
    HexToString(state->rbx, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 360, CursorY, hexBuf, 0x0000FF00);

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 600, CursorY, "RCX: ", 0x00AAAAAA); 
    HexToString(state->rcx, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 660, CursorY, hexBuf, 0x0000FF00);
    CursorY += 8;

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  RDX: ", 0x00AAAAAA);
    HexToString(state->rdx, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 64, CursorY, hexBuf, 0x0000FF00);

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 300, CursorY, "RSI: ", 0x00AAAAAA);
    HexToString(state->rsi, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 360, CursorY, hexBuf, 0x0000FF00);

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 600, CursorY, "RDI: ", 0x00AAAAAA);
    HexToString(state->rdi, hexBuf);
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 660, CursorY, hexBuf, 0x0000FF00);
    CursorY += 16;

    if (CursorY > 700) {
        ClearScreen(); 
        CursorY = 0; 
    }
    UpdateCursor();

    UINT8 scancode = 0;
    while (1) {
        if (inb_debug(0x64) & 0x01) {
            scancode = inb_debug(0x60);
            
            // 1. SPACEBAR RELEASE (0xB9)
            if (scancode == 0xB9) {
                break; 
            }
            
            // 2. 'M' KEY RELEASE (0xB2)
            else if (scancode == 0xB2) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  [+] MEMORY DUMP TRIGGERED (16 Bytes):", 0x00FFFF00);
                CursorY += 8;

                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  Code(RIP): ", 0x00AAAAAA);
                UINT8* rip_ptr = (UINT8*)state->rip;
                for (int i = 0; i < 16; i++) {
                    DrawByteHex(rip_ptr[i], CursorX + 110 + (i * 24), CursorY, 0x00FFFFFF);
                }
                CursorY += 8;

                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  Stack(RSP):", 0x00AAAAAA);
                UINT8* rsp_ptr = (UINT8*)state->rsp;
                for (int i = 0; i < 16; i++) {
                    DrawByteHex(rsp_ptr[i], CursorX + 110 + (i * 24), CursorY, 0x00FF00FF);
                }
                CursorY += 16; 
                
                if (CursorY > 700) { ClearScreen(); }
                UpdateCursor();
            }
            
            // 3. 'W' KEY RELEASE (0x91)
            else if (scancode == 0x91) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  [!] ARMING WATCHPOINT & DISABLING TRAP FLAG...", 0x0000FF00);
                CursorY += 8;
                UpdateCursor();

                ArmHardwareBreakpoint(state->rdi, HW_BP_TYPE_WRITE, HW_BP_LEN_8_BYTES);
                
                state->rflags &= ~(1ULL << 8);
                
                break;
            }

            // 4. 'G' KEY RELEASE (0xA2)
            else if (scancode == 0xA2) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "  [!] DISARMING ALL TRAPS & RUNNING FREE...", 0x00FF0000);
                CursorY += 8; UpdateCursor();

                __asm__ volatile("mov %0, %%dr7" :: "r"(0ULL));
                
                state->rflags &= ~(1ULL << 8);
                
                break;
            }
        }
    }
}

void SetupIDT() {
    for (int i = 0; i < 32; i++) SetIDTEntry(i, FatalWrapper);
    for (int i = 32; i < 256; i++) SetIDTEntry(i, IgnoreWrapper);
    SetIDTEntry(6, PageFaultWrapper);
    SetIDTEntry(8, PageFaultWrapper);
    SetIDTEntry(10, PageFaultWrapper);
    SetIDTEntry(11, PageFaultWrapper);
    SetIDTEntry(12, PageFaultWrapper); 
    SetIDTEntry(13, GPFaultWrapper);
    SetIDTEntry(14, PageFaultWrapper);
    SetIDTEntry(33, KeyboardWrapper);
    SetIDTEntry(1, Isr1Handler); 
    SetIDTEntry(44, MouseWrapper);    
    IDTR.Limit = (sizeof(IDT_ENTRY) * 256) - 1; 
    IDTR.Base = (UINTN)&IDT;
    __asm__ __volatile__("lidt %0" : : "m"(IDTR));
}

__attribute__((aligned(4096))) UINT64 PML4[512];        
__attribute__((aligned(4096))) UINT64 PDPT[512];        
__attribute__((aligned(4096))) UINT64 PD[8][512];       

void SetupPaging() {
    for(int i = 0; i < 512; i++) { PML4[i] = 0; PDPT[i] = 0; }
    for(int i = 0; i < 8; i++) for(int j = 0; j < 512; j++) PD[i][j] = 0;
    PML4[0] = ((UINTN)PDPT) | 0x03;
    for(int i = 0; i < 8; i++) PDPT[i] = ((UINTN)PD[i]) | 0x03;
    UINT64 physical_address = 0;
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 512; j++) {
            PD[i][j] = physical_address | 0x83; 
            physical_address += (2 * 1024 * 1024); 
        }
    }
    __asm__ __volatile__("mov %0, %%cr3" : : "r" ((UINTN)PML4));
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    SystemTable->BootServices->LocateProtocol(&GopGuid, NULL, (void**)&Gop);

    GlobalFrameBuffer = (UINT32*)(UINTN)Gop->Mode->FrameBufferBase;
    GlobalPitch = Gop->Mode->Info->PixelsPerScanLine;
    GlobalWidth = Gop->Mode->Info->HorizontalResolution;
    GlobalHeight = Gop->Mode->Info->VerticalResolution;

    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapKey, DescriptorSize;
    UINT32 DescriptorVersion;

    SystemTable->BootServices->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);

    MemoryMapSize += 2 * DescriptorSize;
    SystemTable->BootServices->AllocatePool(EfiLoaderData, MemoryMapSize, (void**)&MemoryMap);

    SystemTable->BootServices->GetMemoryMap(&MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);

    GlobalUefiMemoryMap = (Tartarus_MemoryDescriptor*)MemoryMap;
    GlobalMMapSize = MemoryMapSize;
    GlobalDescriptorSize = DescriptorSize;

    __asm__ __volatile__("cli");

    SetupGDT(); 
    SetupPIC(); 
    SetupIDT(); 
    SetupPaging(); 
    InitPS2Mouse(); 
    InitHeap(0x10000000, 0x4000000);
    InitFAT32();
    InitApiManifold();

    ClearScreen();
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0, "Tartarus Kernel Modular Baseline Loaded.", 0x00AAAAAA);
    
    CursorY = 16;
    PrintPrompt();
    memset_custom(CommandBuffer, 0, 256);

    SaveMouseBg();
    DrawMouseCursor();

    __asm__ __volatile__("sti");

    while (1) {
        if (MouseMoved) {
            __asm__ __volatile__("cli"); 
            RestoreMouseBg(); 
            PrevMouseX = MouseX; 
            PrevMouseY = MouseY;
            SaveMouseBg(); 
            DrawMouseCursor(); 
            MouseMoved = 0;
            __asm__ __volatile__("sti"); 
        }
        __asm__ __volatile__("hlt"); 
    }
    return EFI_SUCCESS;
}