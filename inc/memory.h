#ifndef MEMORY_H
#define MEMORY_H

#include <efi.h>

void InitHeap(UINT64 heap_start, UINT64 heap_size);

void* malloc(UINTN size);
void free(void* ptr);

typedef struct {
    UINT32 type;
    UINT64 physical_start;
    UINT64 virtual_start;
    UINT64 number_of_pages;
    UINT64 attribute;
} Tartarus_MemoryDescriptor;

void Tartarus_AnalyzePhysicalMemory();

#endif