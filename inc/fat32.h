#ifndef FAT32_H
#define FAT32_H

#include <efi.h>

#pragma pack(push, 1)

// BIOS Parameter Block
typedef struct {
    UINT8  jump[3];
    char   oem[8];
    UINT16 bytes_per_sector;
    UINT8  sectors_per_cluster;
    UINT16 reserved_sectors;
    UINT8  fat_count;
    UINT16 dir_entries;
    UINT16 total_sectors;
    UINT8  media_descriptor;
    UINT16 fat_size_16;
    UINT16 sectors_per_track;
    UINT16 heads;
    UINT32 hidden_sectors;
    UINT32 total_sectors_32;
    
    // FAT32 Extended Fields
    UINT32 fat_size_32;
    UINT16 ext_flags;
    UINT16 fs_version;
    UINT32 root_cluster;
    UINT16 fs_info;
    UINT16 backup_boot_sector;
    UINT8  reserved[12];
    UINT8  drive_number;
    UINT8  reserved1;
    UINT8  boot_signature;
    UINT32 volume_id;
    char   volume_label[11];
    char   fs_type[8];
} FAT32_BPB;

// Directory Entry
typedef struct {
    char   name[11];
    UINT8  attr;
    UINT8  nt_res;
    UINT8  crt_time_tenth;
    UINT16 crt_time;
    UINT16 crt_date;
    UINT16 lst_acc_date;
    UINT16 cluster_hi;
    UINT16 wrt_time;
    UINT16 wrt_date;
    UINT16 cluster_lo;
    UINT32 file_size;
} FAT32_DIR_ENTRY;

#pragma pack(pop)

void InitFAT32();
void ListRootDirectory();
void AutopsyFirstFile();
void ExecuteFirstFile();

#endif