#ifndef DISPLAY_H
#define DISPLAY_H

#include <efi.h>

extern UINT32 *GlobalFrameBuffer;
extern UINT32 GlobalPitch;
extern UINT32 GlobalWidth;
extern UINT32 GlobalHeight;

void DrawChar(UINT32 *fb, UINT32 pitch, int x, int y, char c, UINT32 color);
void DrawString(UINT32 *fb, UINT32 pitch, int x, int y, const char *str, UINT32 color);
void DrawHex64(UINT32 *fb, UINT32 pitch, int x, int y, UINT64 value, UINT32 color);
void EraseCharBlock(UINT32 *fb, UINT32 pitch, int x, int y);
void ScrollScreen();
void UpdateCursor();
void ClearScreen();

#endif