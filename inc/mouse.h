#ifndef MOUSE_H
#define MOUSE_H

#include <efi.h>

extern int MouseMoved;
extern int MouseX;
extern int MouseY;
extern int PrevMouseX;
extern int PrevMouseY;

void InitPS2Mouse();
void MouseHandler();
void SaveMouseBg();
void RestoreMouseBg();
void DrawMouseCursor();

#endif