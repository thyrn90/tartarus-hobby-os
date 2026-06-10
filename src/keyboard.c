#include "keyboard.h"
#include "display.h"
#include "shell.h"

int CapsLock = 0;
int LeftShift = 0;
int RightShift = 0;
int LeftCtrl = 0;

void outb(UINT16 port, UINT8 val) {
    __asm__ __volatile__ ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

UINT8 inb(UINT16 port) {
    UINT8 ret;
    __asm__ __volatile__ ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

char scancode_map[58] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void KeyboardHandler() {
    UINT8 scancode = inb(0x60);
    
    if (scancode & 0x80) {
        UINT8 released = scancode & ~0x80;
        if (released == 0x2A) LeftShift = 0;
        if (released == 0x36) RightShift = 0;
        if (released == 0x1D) LeftCtrl = 0; 
        outb(0x20, 0x20); return; 
    }

    if (scancode == 0x3A) { CapsLock = !CapsLock; }
    else if (scancode == 0x2A) { LeftShift = 1; }
    else if (scancode == 0x36) { RightShift = 1; }
    else if (scancode == 0x1D) { LeftCtrl = 1; } 
    else if (scancode == 0x2F && LeftCtrl) {
        int k = 0;
        while (Clipboard[k] != '\0' && k < 63) {
            if (BufferIndex < 255) {
                CommandBuffer[BufferIndex++] = Clipboard[k];
                DrawChar(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, Clipboard[k], 0x0000AA00); 
                CursorX += 8; UpdateCursor();
            }
            k++;
        }
    }
    else if (scancode == 0x0E) { 
        if (BufferIndex > 0) {
            BufferIndex--; CommandBuffer[BufferIndex] = '\0'; 
            CursorX -= 8;
            if (CursorX < 0) { CursorX = GlobalWidth - 8; CursorY -= 8; }
            EraseCharBlock(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY); 
        }
    } 
    else if (scancode == 0x1C) { 
        CommandBuffer[BufferIndex] = '\0'; 
        ExecuteCommand(); 
    } 
    else if (scancode < 58) { 
        char ascii = scancode_map[scancode];
        if (ascii != 0 && BufferIndex < 255) {
            if (ascii >= 'a' && ascii <= 'z') {
                if (CapsLock ^ (LeftShift || RightShift)) ascii -= 32; 
            }
            CommandBuffer[BufferIndex] = ascii; BufferIndex++;
            DrawChar(GlobalFrameBuffer, GlobalPitch, CursorX, CursorY, ascii, 0x0000AA00); 
            CursorX += 8; UpdateCursor();
        }
    }
    outb(0x20, 0x20);
}