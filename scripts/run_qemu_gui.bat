@echo off
setlocal

set QEMU="C:\MinGW\ucrt64\bin\qemu-system-x86_64.exe"
set KERNEL="build\bzImage"
set INITRAMFS="build\initramfs.cpio.gz"

echo ==========================================================
echo          Launching FeatherOS in Graphical Window          
echo ==========================================================

if not exist %KERNEL% (
    echo [ERROR] Kernel binary %KERNEL% not found!
    exit /b 1
)

if not exist %INITRAMFS% (
    echo [ERROR] Rootfs %INITRAMFS% not found!
    exit /b 1
)

echo Starting FeatherOS in high-res graphical display window...
echo (Press Ctrl+Alt+G to release mouse cursor, or close the window to exit)
echo ----------------------------------------------------------

%QEMU% ^
    -kernel %KERNEL% ^
    -initrd %INITRAMFS% ^
    -m 1024M ^
    -smp 2 ^
    -vga std ^
    -serial mon:stdio ^
    -append "console=tty0 console=ttyS0 rdinit=/init quiet loglevel=3 noapic"

endlocal
