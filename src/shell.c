#include "shell.h"
#include "display.h"
#include "memory.h"
#include "ata.h"
#include "fat32.h"
#include "pci.h"

extern UINT64 Tartarus_ReadMSR(UINT32 msr_index);
extern UINT8 Tartarus_InB(UINT16 port);


int CursorX = 0;
int CursorY = 0;
char CommandBuffer[256];
int BufferIndex = 0;
char Clipboard[64] = {0};

void PrintPrompt() {
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "tartarus> ", 0x00AAAAAA);
    CursorX += 80; 
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
void memset_custom(void *dest, char val, UINTN count) {
    char *temp = (char *)dest;
    for(UINTN i = 0; i < count; i++) temp[i] = val;
}
void strcpy_custom(char *dest, const char *src) {
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}
UINT64 HexToInt64(const char *hexStr) {
    UINT64 result = 0;
    if (hexStr[0] == '0' && (hexStr[1] == 'x' || hexStr[1] == 'X')) hexStr += 2;
    while (*hexStr) {
        char c = *hexStr; UINT64 val = 0;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else break;
        result = (result << 4) | val; hexStr++;
    }
    return result;
}
void HexToString(UINT64 value, char* buffer) {
    char hex_chars[] = "0123456789ABCDEF";
    buffer[0] = '0'; buffer[1] = 'x'; buffer[18] = '\0';
    for (int i = 17; i >= 2; i--) { buffer[i] = hex_chars[value & 0x0F]; value >>= 4; }
}

void ExecuteCommand() {
    CursorX = 0; CursorY += 8; UpdateCursor();
    if (BufferIndex == 0) { PrintPrompt(); return; }

    char cmd[32]; char arg1[64]; char arg2[64];
    memset_custom(cmd, 0, 32); memset_custom(arg1, 0, 64); memset_custom(arg2, 0, 64);

    int i = 0, j = 0;
    while (CommandBuffer[i] != ' ' && CommandBuffer[i] != '\0' && j < 31) cmd[j++] = CommandBuffer[i++];
    if (CommandBuffer[i] == ' ') {
        i++; j = 0;
        while (CommandBuffer[i] != ' ' && CommandBuffer[i] != '\0' && j < 63) arg1[j++] = CommandBuffer[i++];
    }
    if (CommandBuffer[i] == ' ') {
        i++; j = 0;
        while (CommandBuffer[i] != '\0' && j < 63) arg2[j++] = CommandBuffer[i++];
    }

    if (strcmp(cmd, "clear") == 0) { ClearScreen(); } 
    else if (strcmp(cmd, "about") == 0) {
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Tartarus OS - Core Architecture", 0x0000AA00);
        CursorY += 8; UpdateCursor();
    } 
    else if (strcmp(cmd, "sysinfo") == 0) {
        char hexBuf[19];
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "CR3 Base:         ", 0x0000AA00);
        UINT64 cr3_val; __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3_val));
        HexToString(cr3_val, hexBuf);
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 144, CursorY, hexBuf, 0x00AAAAAA);
        CursorY += 8; UpdateCursor();

        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "FrameBuffer Base: ", 0x0000AA00);
        HexToString((UINT64)GlobalFrameBuffer, hexBuf);
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 144, CursorY, hexBuf, 0x00AAAAAA);
        CursorY += 8; UpdateCursor();
        strcpy_custom(Clipboard, hexBuf); 
    }
    else if (strcmp(cmd, "ls") == 0) {
        ListRootDirectory();
    }
    else if (strcmp(cmd, "autopsy") == 0) {
        AutopsyFirstFile();
    }
    else if (strcmp(cmd, "run") == 0) {
        ExecuteFirstFile();
    }
    else if (strcmp(cmd, "read") == 0) {
        if (arg1[0] == '\0') { 
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Usage: read <addr>", 0x00FF0000); 
            CursorY += 8; UpdateCursor(); 
        } 
        else {
            UINT64 target_addr = HexToInt64(arg1);
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Reading [", 0x00AAAAAA);
            char hexBuf[19]; HexToString(target_addr, hexBuf);
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 72, CursorY, hexBuf, 0x0000AA00);
            
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 224, CursorY, "] -> ", 0x00AAAAAA);
            UINT64 *ptr = (UINT64 *)target_addr; HexToString(*ptr, hexBuf);
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 264, CursorY, hexBuf, 0x0000AA00);
            CursorY += 8; UpdateCursor();
            strcpy_custom(Clipboard, hexBuf);
        }
    }

    else if (strcmp(cmd, "alloc") == 0) {
        if (arg1[0] == '\0') {
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Usage: alloc <hex_size>", 0x00FF0000);
            CursorY += 8; UpdateCursor();
        } else {
            UINT64 size = HexToInt64(arg1);
            void *allocated_ptr = malloc(size);
            
            if (allocated_ptr) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Allocated at [", 0x00AAAAAA);
                char hexBuf[19]; HexToString((UINT64)allocated_ptr, hexBuf);
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 112, CursorY, hexBuf, 0x0000FF00);
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 264, CursorY, "]", 0x00AAAAAA);
                strcpy_custom(Clipboard, hexBuf);
            } else {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Allocation FAILED! Out of Memory.", 0x00FF0000);
            }
            CursorY += 8; UpdateCursor();
        }
    }
    else if (strcmp(cmd, "free") == 0) {
        if (arg1[0] == '\0') {
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Usage: free <addr>", 0x00FF0000);
            CursorY += 8; UpdateCursor();
        } else {
            UINT64 addr = HexToInt64(arg1);
            free((void*)addr);
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Memory freed successfully.", 0x0000AA00);
            CursorY += 8; UpdateCursor();
        }
    }
    else if (strcmp(cmd, "diskread") == 0) {
        if (arg1[0] == '\0') {
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Usage: diskread <lba_sector>", 0x00FF0000);
            CursorY += 8; UpdateCursor();
        } else {
            UINT32 target_sector = (UINT32)HexToInt64(arg1);
            
            UINT32 *disk_buffer = (UINT32 *)malloc(512);
            
            if (!disk_buffer) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Buffer allocation failed.", 0x00FF0000);
                CursorY += 8; UpdateCursor();
                return;
            }

            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Requesting sector... ", 0x00AAAAAA);
            CursorY += 8; UpdateCursor();

            // Fire the hardware driver
            if (AtaReadSectors(target_sector, 1, disk_buffer)) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "First 16 bytes of raw data: ", 0x0000FF00);
                CursorY += 8; UpdateCursor();
                
                // Print the first few double-words as a hex dump sample
                for(int d = 0; d < 4; d++) {
                    char hexBuf[19]; HexToString((UINT64)disk_buffer[d], hexBuf);
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, hexBuf, 0x00FFFFFF);
                    CursorX += 160; UpdateCursor();
                }
                CursorX = 0; CursorY += 8; UpdateCursor();
            } else {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Disk read operation failed.", 0x00FF0000);
                CursorY += 8; UpdateCursor();
            }

            free(disk_buffer);
        }
    }
    else if (strcmp(cmd, "write") == 0) {
        if (arg1[0] == '\0' || arg2[0] == '\0') { 
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Usage: write <addr> <val>", 0x00FF0000); 
            CursorY += 8; UpdateCursor(); 
        } 
        else {
            UINT64 target_addr = HexToInt64(arg1);
            UINT64 write_val  = HexToInt64(arg2);
            char hexBuf[19];
            
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Writing ", 0x00AAAAAA);
            HexToString(write_val, hexBuf); DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 64, CursorY, hexBuf, 0x0000AA00);
            
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 216, CursorY, " to [", 0x00AAAAAA);
            HexToString(target_addr, hexBuf); DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 256, CursorY, hexBuf, 0x0000AA00);
            
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 408, CursorY, "]", 0x00AAAAAA);
            CursorY += 8; UpdateCursor();

            UINT64 *ptr = (UINT64 *)target_addr; *ptr = write_val;
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Write successful.", 0x0000AA00);
            CursorY += 8; UpdateCursor();
        }
    }

    else if (strcmp(cmd, "recon") == 0) {
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] INITIATING BARE-METAL HARDWARE RECON...", 0x00FF0000);
            CursorY += 8;

             UINT64 cpu_perf = Tartarus_ReadMSR(0xC0010071);
             DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[RECON] MSR 0x199 (PERF_CTL): ", 0x00FFFF00);
             char hexBuf[19];
             HexToString (cpu_perf, hexBuf);
             DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 250, CursorY, hexBuf, 0x00FFFFFF);
             CursorY += 8;

             UINT8 port_status = Tartarus_InB(0x64);
             DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[RECON] Port 0x64 (STATUS)  : ", 0x00FF00FF);

             char byteBuf[3];
             char hex_chars[] = "0123456789ABCDEF";
             byteBuf[0] = hex_chars[(port_status >> 4) & 0x0F];
             byteBuf[1] = hex_chars [port_status & 0x0F];
             byteBuf[2] = '\0';
             DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 250, CursorY, byteBuf, 0x00FFFFFF);
             CursorY += 8; UpdateCursor();
    }

else if (strcmp(cmd, "crash") == 0) {
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] INITIATING BARE-METAL KERNEL SUICIDE...", 0x00FF0000);
        CursorY += 8; UpdateCursor();

        UINT64 *suicide_ptr = (UINT64 *)0x00000010DEADC0DEULL;
        *suicide_ptr = 0x1337; // boom!
    }
    else if (strcmp(cmd, "pci") == 0) {
        CursorX = 0; 
        
        Tartarus_ScanPCIBus();
    }
    else if (strcmp(cmd, "mem") == 0) {
    CursorX = 0;
    Tartarus_AnalyzePhysicalMemory();
    }
    else {
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "Unknown command.", 0x00FF0000); 
        CursorY += 8; UpdateCursor();
    }

    memset_custom(CommandBuffer, 0, 256); BufferIndex = 0; PrintPrompt();
}