TARGET = BOOTX64.efi
CC = gcc
CFLAGS = -I gnu-efi/inc -I gnu-efi/inc/x86_64 -I gnu-efi/inc/protocol -I inc -ffreestanding -fshort-wchar -mno-red-zone -Wall
LDFLAGS = -shared -Wl,-dll -Wl,--subsystem,10 -e efi_main -nostdlib

all:
	nasm -f win64 src/isr.asm -o src/isr.o
	nasm -f win64 src/trampoline.asm -o src/trampoline.o
	$(CC) $(CFLAGS) $(LDFLAGS) main.c src/*.c src/isr.o src/trampoline.o -o $(TARGET)