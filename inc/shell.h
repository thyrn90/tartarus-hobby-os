#ifndef SHELL_H
#define SHELL_H

#include <efi.h>

extern int CursorX;
extern int CursorY;
extern char CommandBuffer[256];
extern int BufferIndex;
extern char Clipboard[64];

void ExecuteCommand();
void PrintPrompt();

int strcmp(const char *s1, const char *s2);
void memset_custom(void *dest, char val, UINTN count);
void strcpy_custom(char *dest, const char *src);
UINT64 HexToInt64(const char *hexStr);
void HexToString(UINT64 value, char* buffer);

#endif