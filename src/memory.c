#include "memory.h"
#include "display.h"

typedef struct MemoryBlock {
    UINTN size;
    int is_free;
    struct MemoryBlock *next;
} MemoryBlock;

MemoryBlock *heap_head = NULL;

void InitHeap(UINT64 heap_start, UINT64 heap_size) {
    heap_head = (MemoryBlock *)heap_start;
    heap_head->size = heap_size - sizeof(MemoryBlock);
    heap_head->is_free = 1;
    heap_head->next = NULL;
}

void* malloc(UINTN size) {
    if (size == 0) return NULL;

    MemoryBlock *current = heap_head;

    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            
            if (current->size > size + sizeof(MemoryBlock) + 16) {
                MemoryBlock *new_block = (MemoryBlock *)((UINT8*)current + sizeof(MemoryBlock) + size);
                new_block->is_free = 1;
                new_block->size = current->size - size - sizeof(MemoryBlock);
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = 0;
            
            return (void*)((UINT8*)current + sizeof(MemoryBlock)); 
        }
        current = current->next;
    }
    return NULL;
}

void free(void* ptr) {
    if (!ptr) return;
    
    MemoryBlock *block = (MemoryBlock *)((UINT8*)ptr - sizeof(MemoryBlock));
    block->is_free = 1;
    
    if (block->next != NULL && block->next->is_free) {
        block->size += sizeof(MemoryBlock) + block->next->size;
        block->next = block->next->next;
    }
}

extern Tartarus_MemoryDescriptor* GlobalUefiMemoryMap;
extern UINT64 GlobalMMapSize;
extern UINT64 GlobalDescriptorSize;

extern int CursorX;
extern int CursorY;
extern void UpdateCursor();
extern void DrawString(UINT32 *fb, UINT32 pitch, int x, int y, const char *str, UINT32 color);
extern void DrawHex64(UINT32 *fb, UINT32 pitch, int x, int y, UINT64 hex, UINT32 color);

void Tartarus_AnalyzePhysicalMemory() {
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[*] PARSING PHYSICAL UEFI MEMORY MAP...", 0x0000FF00);
    CursorY += 8; UpdateCursor();

    UINT64 total_free_ram = 0;
    UINT64 total_reserved_ram = 0;
    
    UINT64 entries = GlobalMMapSize / GlobalDescriptorSize;

    for (UINT64 i = 0; i < entries; i++) {
        
        Tartarus_MemoryDescriptor* desc = (Tartarus_MemoryDescriptor*)((UINT64)GlobalUefiMemoryMap + (i * GlobalDescriptorSize));

        UINT64 chunk_size = desc->number_of_pages * 4096;

        if (desc->type == 7) {
            total_free_ram += chunk_size;
            
            if (chunk_size >= 1024 * 1024) { 
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "FREE RAM REGION  -> START: ", 0x0000FFFF);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, desc->physical_start, 0x0000FFFF);
                
                DrawString(GlobalFrameBuffer, GlobalPitch, CursorX + 400, CursorY, "SIZE (MB): ", 0x00FFFF00);
                DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 490, CursorY, chunk_size / (1024 * 1024), 0x00FFFF00);
                
                CursorY += 8; UpdateCursor();
            }
        } else {
            total_reserved_ram += chunk_size;
        }
    }

    CursorY += 8;
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "===================================================", 0x00FF00FF);
    CursorY += 8;
    
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[+] USABLE FREE RAM (MB): ", 0x0000FF00);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, total_free_ram / (1024 * 1024), 0x0000FF00);
    CursorY += 8;

    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "[-] RESERVED/YASAK RAM (MB): ", 0x00FF0000);
    DrawHex64(GlobalFrameBuffer, GlobalPitch, CursorX + 220, CursorY, total_reserved_ram / (1024 * 1024), 0x00FF0000);
    CursorY += 8;
    
    DrawString(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, "===================================================", 0x00FF00FF);
    CursorY += 8; UpdateCursor();
}