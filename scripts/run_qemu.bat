@echo off
setlocal

set QEMU="C:\MinGW\ucrt64\bin\qemu-system-x86_64.exe"
set KERNEL="build\bzImage"
set INITRAMFS="build\initramfs.cpio.gz"

echo ==========================================================
echo               Launching FeatherOS in QEMU                 
echo ==========================================================

if not exist %KERNEL% (
    echo [ERROR] Kernel binary %KERNEL% not found!
    echo Please run 'make kernel' or build_kernel.sh first.
    exit /b 1
)

if not exist %INITRAMFS% (
    echo [ERROR] Rootfs %INITRAMFS% not found!
    echo Please run 'make rootfs' or build_rootfs.sh first.
    exit /b 1
)

echo Starting FeatherOS (Press Ctrl+A then X to exit QEMU)...
echo ----------------------------------------------------------

%QEMU% ^
    -kernel %KERNEL% ^
    -initrd %INITRAMFS% ^
    -m 1024M ^
    -smp 2 ^
    -nographic ^
    -serial mon:stdio ^
    -append "console=ttyS0 rdinit=/init quiet loglevel=3 noapic"

endlocal

