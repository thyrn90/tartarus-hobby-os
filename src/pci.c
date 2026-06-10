#include "pci.h"
#include "display.h"

extern int CursorX;
extern int CursorY;
extern void UpdateCursor();

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));

}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Reads a 16-bit word from the specified PCI configuration space
uint16_t Tartarus_PciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;

    // Construct the 32-bit configuration address
    // Bit 31 configuration enable bit is set to 1 (1 << 31)

address = (uint32_t)((lbus << 16) | (lslot << 11) |
          (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    
    // Write the address to the configuration command port
    outl(0xCF8, address);

    // Read the data from the configuration data port
    // Shift by 16 bits if we want the higher word of the 32-bit register
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
    return tmp;
}

// Scans the entire PCI architecture and prints existing hardware
void Tartarus_ScanPCIBus() {
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] SCANNING PCIE HARDWARE BUS...", 0x0000FF00);
    CursorY += 8; UpdateCursor();

    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint16_t vendor = Tartarus_PciConfigReadWord(bus, slot, 0, 0);
            
            if (vendor != 0xFFFF) {
                uint16_t device = Tartarus_PciConfigReadWord(bus, slot, 0, 2);

                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] BUS: ", 0x00FFFFFF);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 70, CursorY, bus, 0x00FFFF00);
                
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 250, CursorY, "SLOT: ", 0x00FFFFFF);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 310, CursorY, slot, 0x00FFFF00);
                
                CursorY += 8;
                
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 30, CursorY, "-> VENDOR: ", 0x00FFAA00);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 120, CursorY, vendor, 0x00FFAA00);
                
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 300, CursorY, "DEVICE: ", 0x0000FFFF);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 370, CursorY, device, 0x0000FFFF);

                CursorY += 16;
                UpdateCursor();
            }
        }
    }
    
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] PCI SCAN COMPLETE RUNNING STABLE.", 0x0000FF00);
    CursorY += 8; UpdateCursor();
}