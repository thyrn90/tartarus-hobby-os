#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <efi.h>

extern int CapsLock;
extern int LeftShift;
extern int RightShift;
extern int LeftCtrl;

void outb(UINT16 port, UINT8 val);
UINT8 inb(UINT16 port);

void KeyboardHandler();

#endif