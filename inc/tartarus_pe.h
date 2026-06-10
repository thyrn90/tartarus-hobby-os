#ifndef TARTARUS_PE_H
#define TARTARUS_PE_H
#define TARTARUS_IMAGE_SIZEOF_SHORT_NAME 8

typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;

typedef struct _TARTARUS_IMAGE_DOS_HEADER {
    UINT16 e_magic;
    UINT16 e_cblp;
    UINT16 e_cp;
    UINT16 e_crlc;
    UINT16 e_cparhdr;
    UINT16 e_minalloc;
    UINT16 e_maxalloc;
    UINT16 e_ss;
    UINT16 e_sp;
    UINT16 e_csum;
    UINT16 e_ip;
    UINT16 e_cs;
    UINT16 e_lfarlc;
    UINT16 e_ovno;
    UINT16 e_res[4];
    UINT16 e_oemid;
    UINT16 e_oeminfo;
    UINT16 e_res2[10];
    UINT32 e_lfanew;
} TARTARUS_IMAGE_DOS_HEADER;

typedef struct _TARTARUS_IMAGE_FILE_HEADER {
    UINT16 Machine;
    UINT16 NumberOfSections;
    UINT32 TimeDateStamp;
    UINT32 PointerToSymbolTable;
    UINT32 NumberOfSymbols;
    UINT16 SizeOfOptionalHeader;
    UINT16 Characteristics;
} TARTARUS_IMAGE_FILE_HEADER;

typedef struct _TARTARUS_IMAGE_DATA_DIRECTORY {
    UINT32 VirtualAddress;
    UINT32 Size;
} TARTARUS_IMAGE_DATA_DIRECTORY;

typedef struct _TARTARUS_IMAGE_OPTIONAL_HEADER64 {
    UINT16 Magic;
    UINT8  MajorLinkerVersion;
    UINT8  MinorLinkerVersion;
    UINT32 SizeOfCode;
    UINT32 SizeOfInitializedData;
    UINT32 SizeOfUninitializedData;
    UINT32 AddressOfEntryPoint;
    UINT32 BaseOfCode;
    UINT64 ImageBase;
    UINT32 SectionAlignment;
    UINT32 FileAlignment;
    UINT16 MajorOperatingSystemVersion;
    UINT16 MinorOperatingSystemVersion;
    UINT16 MajorImageVersion;
    UINT16 MinorImageVersion;
    UINT16 MajorSubsystemVersion;
    UINT16 MinorSubsystemVersion;
    UINT32 Win32VersionValue;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 CheckSum;
    UINT16 Subsystem;
    UINT16 DllCharacteristics;
    UINT64 SizeOfStackReserve;
    UINT64 SizeOfStackCommit;
    UINT64 SizeOfHeapReserve;
    UINT64 SizeOfHeapCommit;
    UINT32 LoaderFlags;
    UINT32 NumberOfRvaAndSizes;
    TARTARUS_IMAGE_DATA_DIRECTORY DataDirectory[16];
} TARTARUS_IMAGE_OPTIONAL_HEADER64;

typedef struct _TARTARUS_IMAGE_IMPORT_DESCRIPTOR {
    union {
        UINT32 Characteristics;
        UINT32 OriginalFirstThunk;
    } DUMMYUNIONNAME;
    UINT32 TimeDateStamp;
    UINT32 ForwarderChain;
    UINT32 Name;
    UINT32 FirstThunk; 
} TARTARUS_IMAGE_IMPORT_DESCRIPTOR;

typedef struct _TARTARUS_IMAGE_IMPORT_BY_NAME {
    UINT16 Hint;
    char Name[1]; 
} TARTARUS_IMAGE_IMPORT_BY_NAME;

typedef struct _TARTARUS_IMAGE_NT_HEADERS64 {
    UINT32 Signature;
    TARTARUS_IMAGE_FILE_HEADER FileHeader;
    TARTARUS_IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} TARTARUS_IMAGE_NT_HEADERS64;

typedef struct _TARTARUS_IMAGE_SECTION_HEADER {
    UINT8  Name[TARTARUS_IMAGE_SIZEOF_SHORT_NAME];
    union {
        UINT32 PhysicalAddress;
        UINT32 VirtualSize;
    } Misc;
    UINT32 VirtualAddress;
    UINT32 SizeOfRawData;
    UINT32 PointerToRawData;
    UINT32 PointerToRelocations;
    UINT32 PointerToLinenumbers;
    UINT16 NumberOfRelocations;
    UINT16 NumberOfLinenumbers;
    UINT32 Characteristics;
} TARTARUS_IMAGE_SECTION_HEADER;

#endif