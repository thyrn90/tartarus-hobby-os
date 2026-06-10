@echo off
:: Create a temporary virtual FAT32 drive for our EFI file
mkdir efi_drive\EFI\BOOT 2>nul

:: Copy your compiled payload as the default boot file
copy BOOTX64.efi efi_drive\EFI\BOOT\BOOTX64.EFI

:: Start QEMU with the absolute path so Windows doesn't get confused
C:\msys64\mingw64\bin\qemu-system-x86_64.exe -bios OVMF.fd -drive format=raw,file=fat:rw:efi_drive -m 1024 -net none -hdb disk.img

echo [thyrn90] QEMU closed. The real host machine remains completely safe.
pause