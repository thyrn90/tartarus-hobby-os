#include "fat32.h"
#include "ata.h"
#include "memory.h"
#include "display.h"
#include "shell.h"
#include "tartarus_pe.h"
#include "api_manifold.h"

int CompareString(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { 
        s1++; 
        s2++; 
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

UINT64 FindOriginalEntryPoint(UINT8* entry_point_addr) {
    
    // MSVC PATTERN SCANNER 
    // sub rsp, 28h -> call -> add rsp, 28h -> jmp __scrt_common_main_seh
    if (entry_point_addr[0] == 0x48 && entry_point_addr[1] == 0x83 && entry_point_addr[2] == 0xEC && entry_point_addr[3] == 0x28) {
        if (entry_point_addr[4] == 0xE8) {
            for (int i = 5; i < 32; i++) {
                if (entry_point_addr[i] == 0xE9) { 
                    INT32 rel_jmp = *(INT32*)(entry_point_addr + i + 1);
                    UINT8* common_main = entry_point_addr + i + 5 + rel_jmp;
                    
                    // Scan inside __scrt_common_main_seh for lea rcx, [main]
                    for (int k = 0; k < 128; k++) {
                        if (common_main[k] == 0x48 && common_main[k+1] == 0x8D && common_main[k+2] == 0x0D) {
                            INT32 main_offset = *(INT32*)(common_main + k + 3);
                            return (UINT64)(common_main + k + 7 + main_offset);
                        }
                    }
                }
            }
        }
    }

    // GCC PATTERN SCANNER 
    // call main -> mov ecx, eax -> call exit
    // We scan the first 2048 bytes of the CRT stub for this exact sequence.
    for (int i = 0; i < 2048; i++) {
        // Check if the current byte is a CALL instruction (0xE8)
        if (entry_point_addr[i] == 0xE8) {
            
            // Check for 32-bit return pattern: mov ecx, eax (89 C1) followed by call (E8)
            if (entry_point_addr[i+5] == 0x89 && entry_point_addr[i+6] == 0xC1 && entry_point_addr[i+7] == 0xE8) {
                // Target Locked! This is the call to GCC's main()
                INT32 rel_call = *(INT32*)(entry_point_addr + i + 1);
                return (UINT64)(entry_point_addr + i + 5 + rel_call);
            }
            
            // Check for 64-bit return pattern: mov rcx, rax (48 89 C1) followed by call (E8)
            if (entry_point_addr[i+5] == 0x48 && entry_point_addr[i+6] == 0x89 && entry_point_addr[i+7] == 0xC1 && entry_point_addr[i+8] == 0xE8) {
                // Target Locked! This is the call to GCC's main()
                INT32 rel_call = *(INT32*)(entry_point_addr + i + 1);
                return (UINT64)(entry_point_addr + i + 5 + rel_call);
            }
        }
    }

    return (UINT64)entry_point_addr;
}

// Global variables to store partition offsets
UINT32 fat_start_sector;
UINT32 data_start_sector;
UINT32 root_cluster;
UINT8  sectors_per_cluster;

void InitFAT32() {
    UINT32 *boot_sector_buffer = (UINT32 *)malloc(512);
    if (!boot_sector_buffer) return;

    // Read Sector 0
    if (AtaReadSectors(0, 1, boot_sector_buffer)) {
        FAT32_BPB *bpb = (FAT32_BPB *)boot_sector_buffer;

        // Calculate the critical zones of the disk using the BPB Math
        fat_start_sector = bpb->reserved_sectors;
        data_start_sector = fat_start_sector + (bpb->fat_count * bpb->fat_size_32);
        root_cluster = bpb->root_cluster;
        sectors_per_cluster = bpb->sectors_per_cluster;
        
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "FAT32 Volume Mounted Successfully!", 0x0000FF00);
        CursorY += 8; UpdateCursor();
    }
    free(boot_sector_buffer);
}


UINT32 GetNextCluster(UINT32 current_cluster) {
    UINT32 fat_sector = fat_start_sector + ((current_cluster * 4) / 512);
    UINT32 fat_offset = (current_cluster * 4) % 512;
    
    UINT32 *fat_buffer = (UINT32 *)malloc(512);
    if (!fat_buffer || !AtaReadSectors(fat_sector, 1, fat_buffer)) {
        if(fat_buffer) free(fat_buffer);
        return 0x0FFFFFFF;
    }
    
    UINT32 next_cluster = *(UINT32*)((UINT8*)fat_buffer + fat_offset) & 0x0FFFFFFF;
    free(fat_buffer);
    
    return next_cluster;
}

void AutopsyFirstFile() {
    UINT32 root_sector = data_start_sector + ((root_cluster - 2) * sectors_per_cluster);
    UINT32 *dir_buffer = (UINT32 *)malloc(512 * sectors_per_cluster);
    if (!dir_buffer) return;

    if (AtaReadSectors(root_sector, sectors_per_cluster, dir_buffer)) {
        FAT32_DIR_ENTRY *entry = (FAT32_DIR_ENTRY *)dir_buffer;
        int max_entries = (512 * sectors_per_cluster) / 32;

        for (int i = 0; i < max_entries; i++) {
            if (entry[i].name[0] == 0x00) break;
            
            // Skip deleted files (0xE5), Directories (0x10), and Long File Names (0x0F)
            if ((unsigned char)entry[i].name[0] == 0xE5 || entry[i].attr == 0x10 || entry[i].attr == 0x0F) continue; 

            UINT32 file_cluster = ((UINT32)entry[i].cluster_hi << 16) | entry[i].cluster_lo;
            
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] Executing Autopsy on file...", 0x00FF0000);
            CursorY += 8; UpdateCursor();

            UINT32 current_cluster = file_cluster;
            UINT32 bytes_read = 0;
            UINT32 total_size = entry[i].file_size;

            while (current_cluster < 0x0FFFFFF8 && bytes_read < total_size) {
                UINT32 sector = data_start_sector + ((current_cluster - 2) * sectors_per_cluster);
                UINT32 *cluster_buffer = (UINT32 *)malloc(512 * sectors_per_cluster);
                
                if (AtaReadSectors(sector, sectors_per_cluster, cluster_buffer)) {
                    char* byte_ptr = (char*)cluster_buffer;
                    
                    for (UINT32 b = 0; b < (512 * sectors_per_cluster) && bytes_read < total_size; b++) {
                        DrawChar(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, byte_ptr[b], 0x00FFFF00);
                        CursorX += 8; UpdateCursor();
                        
                        if (CursorX > 700) { CursorX = 0; CursorY += 8; }
                        bytes_read++;
                    }
                }
                free(cluster_buffer);
                
                current_cluster = GetNextCluster(current_cluster);
            }
            
            CursorX = 0; CursorY += 8; UpdateCursor();
            free(dir_buffer);
            return;
        }
    }
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "No file found for autopsy.", 0x00FF0000);
    CursorY += 8; UpdateCursor();
    free(dir_buffer);
}

void ListRootDirectory() {
    // Calculate the exact LBA sector where the Root Directory starts
    UINT32 root_sector = data_start_sector + ((root_cluster - 2) * sectors_per_cluster);
    
    // Allocate memory to read the cluster
    UINT32 *dir_buffer = (UINT32 *)malloc(512 * sectors_per_cluster);
    if (!dir_buffer) return;

    if (AtaReadSectors(root_sector, sectors_per_cluster, dir_buffer)) {
        FAT32_DIR_ENTRY *entry = (FAT32_DIR_ENTRY *)dir_buffer;
        
        // 512 bytes / 32 bytes per entry = 16 entries per sector
        int max_entries = (512 * sectors_per_cluster) / 32; 
        
        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "--- ROOT DIRECTORY ---", 0x00AAAAAA);
        CursorY += 8; UpdateCursor();

        for (int i = 0; i < max_entries; i++) {
            // 0x00 means no more entries, 0xE5 means deleted file
            if (entry[i].name[0] == 0x00) break; 
            if ((unsigned char)entry[i].name[0] == 0xE5) continue;
            // Skip Long File Name
            if (entry[i].attr == 0x0F) continue;

            // Extract the 11-character 8.3 filename
            char filename[12];
            for (int j = 0; j < 11; j++) filename[j] = entry[i].name[j];
            filename[11] = '\0';

            // Print filename and size
            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, filename, 0x00FFFFFF);
            
            if (entry[i].attr & 0x10) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 100, CursorY, "<DIR>", 0x00FFFF00);
            } else {
                char sizeBuf[19];
                HexToString(entry[i].file_size, sizeBuf);
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 100, CursorY, sizeBuf, 0x00AAAAAA);
            }
            
            CursorY += 8; UpdateCursor();
        }
    }
    free(dir_buffer);
}

void ExecuteFirstFile() {
    UINT32 root_sector = data_start_sector + ((root_cluster - 2) * sectors_per_cluster);
    UINT32 *dir_buffer = (UINT32 *)malloc(512 * sectors_per_cluster);
    if (!dir_buffer) return;

    if (AtaReadSectors(root_sector, sectors_per_cluster, dir_buffer)) {
        FAT32_DIR_ENTRY *entry = (FAT32_DIR_ENTRY *)dir_buffer;
        int max_entries = (512 * sectors_per_cluster) / 32;

        for (int i = 0; i < max_entries; i++) {
            if (entry[i].name[0] == 0x00) break;
            if ((unsigned char)entry[i].name[0] == 0xE5 || entry[i].attr == 0x10 || entry[i].attr == 0x0F) continue; 

            UINT32 total_size = entry[i].file_size;
            void* exec_buffer = malloc(total_size + 512); 
            
            UINT32 file_cluster = ((UINT32)entry[i].cluster_hi << 16) | entry[i].cluster_lo;
            UINT32 current_cluster = file_cluster;
            UINT32 bytes_read = 0;

            while (current_cluster < 0x0FFFFFF8 && bytes_read < total_size) {
                UINT32 sector = data_start_sector + ((current_cluster - 2) * sectors_per_cluster);
                AtaReadSectors(sector, sectors_per_cluster, (UINT32*)((UINT8*)exec_buffer + bytes_read));
                bytes_read += (512 * sectors_per_cluster);
                current_cluster = GetNextCluster(current_cluster);
            }

            DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] Analyzing File Signature...", 0x00AAAAAA);
            CursorY += 8; UpdateCursor();

            UINT8 *buf = (UINT8*)exec_buffer;
            
            // Check for Windows PE
            if (buf[0] == 0x4D && buf[1] == 0x5A) {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Windows PE (.exe) Detected! Parsing Headers...", 0x00FF00FF);
                CursorY += 8; UpdateCursor();
                
                TARTARUS_IMAGE_DOS_HEADER* dosHeader = (TARTARUS_IMAGE_DOS_HEADER*)buf;
                // Jump to the NT Headers using the offset provided by e_lfanew
                TARTARUS_IMAGE_NT_HEADERS64* ntHeaders = (TARTARUS_IMAGE_NT_HEADERS64*)(buf + dosHeader->e_lfanew);

                // Verify the "PE\0\0" signature
                if (ntHeaders->Signature == 0x00004550) {
                    char hexBuf[19];
                    
                    // Extract and display the requested RAM Base Address
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[-] Preferred Image Base: ", 0x00AAAAAA);
                    HexToString(ntHeaders->OptionalHeader.ImageBase, hexBuf);
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, hexBuf, 0x00FFFFFF);
                    CursorY += 8; UpdateCursor();

                    // Extract and display the actual Entry Point of the C++ Code
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[-] Address Of Entry Point: ", 0x00AAAAAA);
                    HexToString(ntHeaders->OptionalHeader.AddressOfEntryPoint, hexBuf);
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, hexBuf, 0x0000FF00);
                    CursorY += 8; UpdateCursor();
                    
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] Header parsed successfully. Section mapping required next.", 0x00FFFF00);
                    CursorY += 8; UpdateCursor();

                    // SECTION MAPPING & EXECUTION ENGINE
                    
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Mapping Sections to Virtual RAM...", 0x00FF00FF);
                    CursorY += 8; UpdateCursor();

                    // Allocate a 2 Megabyte "Virtual RAM Sandbox" for the Executable
                    void* virtual_ram = malloc(2 * 1024 * 1024); 
                    
                    TARTARUS_IMAGE_SECTION_HEADER* sectionHeader = (TARTARUS_IMAGE_SECTION_HEADER*)(
                        (UINT8*)ntHeaders + 
                        sizeof(UINT32) + 
                        sizeof(TARTARUS_IMAGE_FILE_HEADER) + 
                        ntHeaders->FileHeader.SizeOfOptionalHeader
                    );

                    // Iterate through sections and copy them into Virtual RAM
                    UINT16 numSections = ntHeaders->FileHeader.NumberOfSections;
                    for (UINT16 s = 0; s < numSections; s++) {
                        if (sectionHeader[s].SizeOfRawData > 0) {
                            UINT8* dest = (UINT8*)virtual_ram + sectionHeader[s].VirtualAddress;
                            UINT8* src = (UINT8*)buf + sectionHeader[s].PointerToRawData;
                            
                            // Bare-metal memory copy
                            for(UINT32 b = 0; b < sectionHeader[s].SizeOfRawData; b++) {
                                dest[b] = src[b];
                            }
                        }
                    }

                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Patching Import Address Table (IAT)...", 0x00FF00FF);
                    CursorY += 8; UpdateCursor();
    
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Sections Mapped. Executing Entry Point...", 0x0000FF00);
                    CursorY += 8; UpdateCursor();

                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Sections Mapped. Analyzing Compiler Signatures...", 0x0000FF00);
                    CursorY += 8; UpdateCursor();

                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Resolving Static IAT Imports...", 0x00FF00FF);
                        CursorY += 8; UpdateCursor();

                        UINT32 importDirRVA = ntHeaders->OptionalHeader.DataDirectory[1].VirtualAddress;
                        if (importDirRVA != 0) {
                            TARTARUS_IMAGE_IMPORT_DESCRIPTOR* importDesc = (TARTARUS_IMAGE_IMPORT_DESCRIPTOR*)((UINT8*)virtual_ram + importDirRVA);
                            
                            UINT8* iat_tramp_pool = (UINT8*)0x04000000; 
                            memset_custom(iat_tramp_pool, 0, 0x5000);
                            UINT32 tramp_offset = 0;

                            while (importDesc->Name != 0) {
                                UINT64* thunkRef = (UINT64*)((UINT8*)virtual_ram + importDesc->OriginalFirstThunk);
                                UINT64* funcRef = (UINT64*)((UINT8*)virtual_ram + importDesc->FirstThunk);
                                if (importDesc->OriginalFirstThunk == 0) thunkRef = funcRef; 

                                while (*thunkRef != 0) {
                                    if ((*thunkRef & 0x8000000000000000) == 0) { 
                                        UINT8* nameData = (UINT8*)virtual_ram + (*thunkRef & 0x7FFFFFFF);
                                        char* funcName = (char*)(nameData + 2); 
                                        
                                        UINT8* tramp = iat_tramp_pool + tramp_offset;
                                        
                                        tramp[0] = 0x48; tramp[1] = 0xB9;
                                        *(UINT64*)(tramp + 2) = (UINT64)funcName;
                                        
                                        // mov rax, StaticIatTrap
                                        tramp[10] = 0x48; tramp[11] = 0xB8;
                                        extern UINT64 StaticIatTrap(char*);
                                        *(UINT64*)(tramp + 12) = (UINT64)&StaticIatTrap;
                                        
                                        // jmp rax
                                        tramp[20] = 0xFF; tramp[21] = 0xE0;
                                        
                                        *funcRef = (UINT64)tramp;
                                        tramp_offset += 32;
                                    }
                                    thunkRef++; funcRef++;
                                }
                                importDesc++;
                            }
                        }

                    UINT8* raw_entry = (UINT8*)virtual_ram + ntHeaders->OptionalHeader.AddressOfEntryPoint;
                    
                    UINT64 real_main_oep = FindOriginalEntryPoint(raw_entry);
                    
                    if (real_main_oep != (UINT64)raw_entry) {
                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Automated OEP Found! Bypassed CRT Stub.", 0x0000FF00);
                        CursorY += 8; UpdateCursor();
                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[-] Target main() Address: ", 0x00AAAAAA);
                        HexToString(real_main_oep, hexBuf);
                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, hexBuf, 0x00FFFF00);
                        CursorY += 8; UpdateCursor();
                    } else {
                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Compiler pattern mismatch. Defaulting to raw EntryPoint.", 0x00FFFF00);
                        CursorY += 8; UpdateCursor();
                    }

                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] Arming Trap Flag. Firing Execution...", 0x00FFFF00);
                    CursorY += 8; UpdateCursor();

                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Deploying TARTARUS God Object at 0x02000000...", 0x00FF00FF);
                        CursorY += 8; UpdateCursor();
                        
                        extern void Tartarus_DeployGodObject();
                        Tartarus_DeployGodObject();
                        
                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Constructing Multi-Module PEB/LDR Chain...", 0x00FF00FF);
                        CursorY += 8; UpdateCursor();

                        UINT8* fake_peb = (UINT8*)0x5000; memset_custom(fake_peb, 0, 0x100);
                        UINT8* fake_ldr = (UINT8*)0x6000; memset_custom(fake_ldr, 0, 0x100);
                        *(UINT64*)(fake_peb + 0x18) = (UINT64)fake_ldr;

                        UINT8* entry1_ntdll   = (UINT8*)0x7000; memset_custom(entry1_ntdll, 0, 0x100);
                        UINT8* entry2_kernel  = (UINT8*)0x8000; memset_custom(entry2_kernel, 0, 0x100);
                        *(UINT64*)(fake_ldr + 0x20) = (UINT64)(entry1_ntdll + 0x10);
                        *(UINT64*)(entry1_ntdll + 0x10) = (UINT64)(entry2_kernel + 0x10); 
                        *(UINT64*)(entry1_ntdll + 0x18) = (UINT64)(fake_ldr + 0x20);      
                        *(UINT64*)(entry2_kernel + 0x10) = (UINT64)(fake_ldr + 0x20);      
                        *(UINT64*)(entry2_kernel + 0x18) = (UINT64)(entry1_ntdll + 0x10); 
                        *(UINT64*)(entry1_ntdll + 0x30) = 0x0000000002000000; 
                        
                        UINT16* name_ntdll = (UINT16*)0x7500;
                        name_ntdll[0]='n'; name_ntdll[1]='t'; name_ntdll[2]='d'; name_ntdll[3]='l';
                        name_ntdll[4]='l'; name_ntdll[5]='.'; name_ntdll[6]='d'; name_ntdll[7]='l'; name_ntdll[8]='l'; name_ntdll[9]='\0';

                        *(UINT16*)(entry1_ntdll + 0x48) = 18; *(UINT16*)(entry1_ntdll + 0x4A) = 20; 
                        *(UINT64*)(entry1_ntdll + 0x50) = (UINT64)name_ntdll; 
                        *(UINT16*)(entry1_ntdll + 0x58) = 18; *(UINT16*)(entry1_ntdll + 0x5A) = 20; 
                        *(UINT64*)(entry1_ntdll + 0x60) = (UINT64)name_ntdll;
                        *(UINT64*)(entry2_kernel + 0x30) = 0x0000000003000000; 

                        UINT16* name_kernel = (UINT16*)0x8500;
                        name_kernel[0]='k'; name_kernel[1]='e'; name_kernel[2]='r'; name_kernel[3]='n';
                        name_kernel[4]='e'; name_kernel[5]='l'; name_kernel[6]='3'; name_kernel[7]='2';
                        name_kernel[8]='.'; name_kernel[9]='d'; name_kernel[10]='l'; name_kernel[11]='l'; name_kernel[12]='\0';

                        *(UINT16*)(entry2_kernel + 0x48) = 24; *(UINT16*)(entry2_kernel + 0x4A) = 26; 
                        *(UINT64*)(entry2_kernel + 0x50) = (UINT64)name_kernel; 
                        *(UINT16*)(entry2_kernel + 0x58) = 24; *(UINT16*)(entry2_kernel + 0x5A) = 26; 
                        *(UINT64*)(entry2_kernel + 0x60) = (UINT64)name_kernel; 

                        UINT8* fake_teb = (UINT8*)0x4000; memset_custom(fake_teb, 0, 0x100);
                        *(UINT64*)(fake_teb + 0x60) = (UINT64)fake_peb; 
                        *(UINT64*)0x30 = (UINT64)fake_teb; 
                        *(UINT64*)0x60 = (UINT64)fake_peb;

                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Deploying NTDLL and KERNEL32 Manifolds...", 0x00FF00FF);
                        CursorY += 8; UpdateCursor();

                        UINT8* ntdll_memory = (UINT8*)0x02000000;
                        memset_custom(ntdll_memory, 0, 0x5000);
                        BuildUniversalExportTable(ntdll_memory, "ntdll.dll"); 

                        UINT8* kernel32_memory = (UINT8*)0x03000000;
                        memset_custom(kernel32_memory, 0, 0x5000);
                        BuildUniversalExportTable(kernel32_memory, "KERNEL32.DLL");

                        DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Resetting CPU MSR Registers (FS/GS Base)...", 0x0000FF00);
                        CursorY += 8; UpdateCursor();

                        __asm__ volatile("wrmsr" : : "c"(0xC0000100), "a"(0), "d"(0));
                        __asm__ volatile("wrmsr" : : "c"(0xC0000101), "a"(0), "d"(0));
                        __asm__ volatile("wrmsr" : : "c"(0xC0000102), "a"(0), "d"(0));

                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Planting TARTARUS ABYSS TRAP at physical address 0x0...", 0x0000FF00);
                    CursorY += 8; UpdateCursor();

                    UINT8* abyss_page = (UINT8*)0x00000000;
                    extern UINT64 UniversalApiTrap(UINT32);

                    abyss_page[0] = 0xB9; *(UINT32*)(abyss_page + 1) = 999; 
                    
                    abyss_page[5] = 0x48; abyss_page[6] = 0xB8;             
                    *(UINT64*)(abyss_page + 7) = (UINT64)&UniversalApiTrap;
                    
                    abyss_page[15] = 0xFF; abyss_page[16] = 0xE0;
                    extern void ExecuteWithTrapFlag(void* payload_address);
                    ExecuteWithTrapFlag((void*)real_main_oep);
                    
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] PE Execution Completed.", 0x0000FF00);

                } else {
                    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[X] Invalid PE Signature!", 0x00FF0000);
                }
            }
            else if (buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[!] Linux ELF Binary Detected!", 0x0000FFFF);
                CursorY += 8; UpdateCursor();
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[-] Full ELF Segment Mapping required to execute.", 0x00FF0000);
            }
            else {
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Raw Binary Detected. Jumping to Ring 0 with TF...", 0x0000FF00);
                CursorY += 8; UpdateCursor();

                extern void ExecuteWithTrapFlag(void* payload_address);
                
                ExecuteWithTrapFlag(exec_buffer);

                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] Payload Execution Completed.", 0x0000FF00);
            }
            CursorX = 0; CursorY += 8; UpdateCursor();
            free(exec_buffer);
            free(dir_buffer);
            return;
        }
    }
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "No executable found.", 0x00FF0000);
    CursorY += 8; UpdateCursor();
    free(dir_buffer);
}