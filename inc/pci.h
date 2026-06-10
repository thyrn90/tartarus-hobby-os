#ifndef PCI_H
#define PCI_H

#include <stdint.h>

uint16_t Tartarus_PciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void Tartarus_ScanPCIBus();

#endif