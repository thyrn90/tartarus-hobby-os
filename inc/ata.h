#ifndef ATA_H
#define ATA_H

#include <efi.h>

int AtaReadSectors(UINT32 lba, UINT8 sector_count, UINT32 *target_buffer);

#endif