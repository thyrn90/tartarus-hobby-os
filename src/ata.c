#include "ata.h"
#include "keyboard.h" 

static inline UINT16 inw(UINT16 port) {
    UINT16 ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int AtaWaitReady() {
    int timeout = 100000;
    while (--timeout) {
        UINT8 status = inb(0x1F7);
        if (!(status & 0x80) && (status & 0x08)) return 1; 
        
        if (status & 0x01) return 0; 
    }
    return 0;
}

int AtaReadSectors(UINT32 lba, UINT8 sector_count, UINT32 *target_buffer) {
    outb(0x1F6, 0xF0 | ((lba >> 24) & 0x0F));
    outb(0x1F1, 0x00);
    outb(0x1F2, sector_count);
    outb(0x1F3, (UINT8)lba);
    outb(0x1F4, (UINT8)(lba >> 8));
    outb(0x1F5, (UINT8)(lba >> 16));
    outb(0x1F7, 0x20);

    UINT16 *ptr = (UINT16 *)target_buffer;

    for (int s = 0; s < sector_count; s++) {
        if (!AtaWaitReady()) return 0;

        for (int i = 0; i < 256; i++) {
            *ptr = inw(0x1F0);
            ptr++;
        }
    }
    return 1;
}