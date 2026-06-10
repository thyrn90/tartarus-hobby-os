#include "api_manifold.h"
#include "display.h"

char GlobalLastAPI[128] = "NULL";

const char* Tartarus_Malware_API_List[] = {
    "AdjustTokenPrivileges", "CloseHandle", "CreateFileA", "CreateFileW",
    "CreateMutexA", "CreateProcessA", "CreateRemoteThread", "CreateThread",
    "CreateToolhelp32Snapshot", "CryptAcquireContextA", "DeviceIoControl",
    "ExitProcess", "FindWindowA", "FreeLibrary", "GetCommandLineA",
    "GetCurrentProcess", "GetModuleHandleA", "GetProcAddress",
    "GetSystemDirectoryA", "GetTickCount", "GetWindowThreadProcessId",
    "InternetCloseHandle", "InternetOpenA", "InternetOpenUrlA", "InternetReadFile",
    "IsDebuggerPresent", "LdrGetProcedureAddress", "LdrLoadDll", "LoadLibraryA",
    "LoadLibraryExA", "LoadResource", "LockResource", "MapViewOfFile",
    "Module32First", "Module32Next", "NtAllocateVirtualMemory",
    "NtProtectVirtualMemory", "NtQueryInformationProcess", "NtUnmapViewOfSection",
    "NtWriteVirtualMemory", "OpenProcess", "OpenProcessToken",
    "OutputDebugStringA", "Process32First", "Process32Next", "ReadProcessMemory",
    "RegCloseKey", "RegOpenKeyExA", "RegQueryValueExA", "RegSetValueExA",
    "ResumeThread", "RtlDecompressBuffer", "SetFilePointer", "SetUnhandledExceptionFilter",
    "SetWindowsHookExA", "Sleep", "TerminateProcess", "URLDownloadToFileA",
    "VirtualAlloc", "VirtualAllocEx", "VirtualFree", "VirtualProtect",
    "VirtualProtectEx", "WaitForSingleObject", "WriteFile", "WriteProcessMemory",
    "lstrcmpA", "lstrcmpiA", "lstrcpyA", "lstrlenA"
};

#define API_COUNT (sizeof(Tartarus_Malware_API_List) / sizeof(const char*))

void InitApiManifold() {
    GlobalLastAPI[0] = 'N'; GlobalLastAPI[1] = 'U'; GlobalLastAPI[2] = 'L'; GlobalLastAPI[3] = 'L'; GlobalLastAPI[4] = '\0';
}

void CopyApiName(const char* src, char* dest) {
    int i = 0;
    while (src[i] != '\0' && i < 127) { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

UINT64 UniversalApiTrap(UINT32 api_index) {
    if (api_index == 999) {
        CopyApiName("ABYSS TRAP: MALWARE JUMPED TO NULL (NOT FOUND)", GlobalLastAPI);
    } else if (api_index < API_COUNT) {
        CopyApiName(Tartarus_Malware_API_List[api_index], GlobalLastAPI);
    }

    ClearScreen();
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 0,  "===================================================", 0x00FF00FF);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 8,  "  [TARTARUS] UNIVERSAL API MANIFOLD TRIGGERED      ", 0x00FF00FF);
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 16, "===================================================", 0x00FF00FF);
    
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 32, "[+] Malware successfully caught calling API: ", 0x0000FF00);
    DrawString(GlobalFrameBuffer, GlobalPitch, 350, 32, GlobalLastAPI, 0x00FFFFFF);

    DrawString(GlobalFrameBuffer, GlobalPitch, 0, 64, "[!] EXECUTION HALTED. MALWARE DISARMED.", 0x00FF0000);

    while(1) { __asm__ volatile("cli; hlt"); }
    return 0;
}

void BuildUniversalExportTable(UINT8* fake_dll_base, const char* dll_internal_name) {
    *(UINT16*)(fake_dll_base) = 0x5A4D; 
    *(UINT32*)(fake_dll_base + 0x3C) = 0x40; 
    *(UINT32*)(fake_dll_base + 0x40) = 0x00004550; 
    *(UINT16*)(fake_dll_base + 0x54) = 0xF0;   
    *(UINT16*)(fake_dll_base + 0x58) = 0x020B; 
    *(UINT32*)(fake_dll_base + 0xC8) = 0x100;  
    *(UINT32*)(fake_dll_base + 0xCC) = 0x100;  

    UINT8* exp_dir = fake_dll_base + 0x100;
    *(UINT32*)(exp_dir + 0x0C) = 0x180; 
    *(UINT32*)(exp_dir + 0x14) = API_COUNT; 
    *(UINT32*)(exp_dir + 0x18) = API_COUNT; 
    
    UINT32 func_array_rva = 0x200; 
    UINT32 name_array_rva = 0x400; 
    UINT32 ord_array_rva  = 0x600; 
    UINT32 string_pool_rva = 0x800;
    UINT32 tramp_pool_rva  = 0x2000;

    *(UINT32*)(exp_dir + 0x1C) = func_array_rva; 
    *(UINT32*)(exp_dir + 0x20) = name_array_rva; 
    *(UINT32*)(exp_dir + 0x24) = ord_array_rva; 

    char* internal_name = (char*)(fake_dll_base + 0x180);
    CopyApiName(dll_internal_name, internal_name);

    for (UINT32 i = 0; i < API_COUNT; i++) {
        char* str_dest = (char*)(fake_dll_base + string_pool_rva);
        CopyApiName(Tartarus_Malware_API_List[i], str_dest);
        
        *(UINT32*)(fake_dll_base + name_array_rva + (i * 4)) = string_pool_rva;
        *(UINT16*)(fake_dll_base + ord_array_rva + (i * 2)) = i;

        UINT8* tramp = fake_dll_base + tramp_pool_rva;
        tramp[0] = 0xB9; *(UINT32*)(tramp + 1) = i; // mov ecx, i
        tramp[5] = 0x48; tramp[6] = 0xB8;           // mov rax, UniversalApiTrap
        *(UINT64*)(tramp + 7) = (UINT64)&UniversalApiTrap;
        tramp[15] = 0xFF; tramp[16] = 0xE0;         // jmp rax

        *(UINT32*)(fake_dll_base + func_array_rva + (i * 4)) = tramp_pool_rva;

        string_pool_rva += 64; 
        tramp_pool_rva += 32;  
    }
}

UINT32 Tartarus_LogCursorY = 100;

extern UINT32 CursorY;
extern void UpdateCursor(void);

void __attribute__((naked)) Tartarus_AmnesiaStub() {
    __asm__ volatile(
        "xor %rax, %rax\n\t" 
        "ret\n\t"            
    );
}

void Tartarus_DeployGodObject() {
    UINT64* god_zone = (UINT64*)0x06000000;
    UINT64 amnesia_address = (UINT64)&Tartarus_AmnesiaStub;
    
    for(int i = 0; i < 4096; i++) {
        god_zone[i] = amnesia_address;
    }
}

int Tartarus_IsAntiDebug(char* name) {
    // "IsDebuggerPresent"
    if (name[0] == 'I' && name[1] == 's' && name[2] == 'D') return 1;
    // "CheckRemoteDebuggerPresent"
    if (name[0] == 'C' && name[1] == 'h' && name[2] == 'e' && name[3] == 'c') return 1;
    return 0; 
}

UINT64 StaticIatTrap(char* funcName) {
    DrawString(GlobalFrameBuffer, GlobalPitch, 0, CursorY, "[X-RAY] API Intercepted: ", 0x00FFFF00);
    DrawString(GlobalFrameBuffer, GlobalPitch, 220, CursorY, funcName, 0x00FFFFFF);
    
    CursorY += 8; 
    UpdateCursor(); 

    if (Tartarus_IsAntiDebug(funcName)) {
        return 0; 
    }

    return 0x0000000006000000; 
}