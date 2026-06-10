#include "mouse.h"
#include "keyboard.h"
#include "display.h"

int MouseX = 500; 
int MouseY = 500;
int PrevMouseX = 500; 
int PrevMouseY = 500;
int MouseMoved = 0;
UINT8 mouse_cycle = 0; 
INT8 mouse_byte[3];

void MouseWait(UINT8 a_type) {
    UINT32 _time_out = 100000;
    if (a_type == 0) { while (_time_out--) { if ((inb(0x64) & 1) == 1) return; } } 
    else { while (_time_out--) { if ((inb(0x64) & 2) == 0) return; } }
}
void MouseWrite(UINT8 a_write) { MouseWait(1); outb(0x64, 0xD4); MouseWait(1); outb(0x60, a_write); }
UINT8 MouseRead() { MouseWait(0); return inb(0x60); }

void InitPS2Mouse() {
    MouseWait(1); outb(0x64, 0xA8); 
    MouseWait(1); outb(0x64, 0x20); 
    MouseWait(0); UINT8 status = (inb(0x60) | 2); 
    MouseWait(1); outb(0x64, 0x60); 
    MouseWait(1); outb(0x60, status);
    MouseWrite(0xF6); MouseRead();  
    MouseWrite(0xF4); MouseRead();  
}

void MouseHandler() {
    UINT8 status = inb(0x64);
    if (!(status & 0x20)) { outb(0x20, 0x20); outb(0xA0, 0x20); return; } 
    mouse_byte[mouse_cycle++] = inb(0x60); 
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40)) { }
        else {
            MouseX += mouse_byte[1]; MouseY -= mouse_byte[2]; 
            if (MouseX < 0) { MouseX = 0; }
            if (MouseY < 0) { MouseY = 0; }
            if (MouseX >= GlobalWidth - 5) { MouseX = GlobalWidth - 5; }
            if (MouseY >= GlobalHeight - 5) { MouseY = GlobalHeight - 5; }
            MouseMoved = 1; 
        }
    }
    outb(0xA0, 0x20); outb(0x20, 0x20);
}