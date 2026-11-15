#!/usr/bin/env bash
# FeatherOS QEMU Virtual Machine Runner
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
KERNEL="${ROOT_DIR}/build/bzImage"
INITRAMFS="${ROOT_DIR}/build/initramfs.cpio.gz"

echo "=========================================================="
echo "               Launching FeatherOS in QEMU                "
echo "=========================================================="

if [ ! -f "${KERNEL}" ]; then
    echo " [✗] Error: Kernel image '${KERNEL}' not found!"
    echo "     Please run './scripts/build_kernel.sh build' first."
    exit 1
fi

if [ ! -f "${INITRAMFS}" ]; then
    echo " [✗] Error: Rootfs image '${INITRAMFS}' not found!"
    echo "     Please run './scripts/build_rootfs.sh' first."
    exit 1
fi

ACCEL_FLAGS=""
if [ -e /dev/kvm ] && [ -w /dev/kvm ]; then
    ACCEL_FLAGS="-enable-kvm -cpu host"
fi

echo " [*] Booting FeatherOS..."
echo "     (To exit QEMU, press Ctrl+A then X)"
echo "----------------------------------------------------------"

qemu-system-x86_64 \
    ${ACCEL_FLAGS} \
    -kernel "${KERNEL}" \
    -initrd "${INITRAMFS}" \
    -m 1024M \
    -smp 2 \
    -nographic \
    -serial mon:stdio \
    -append "console=ttyS0 rdinit=/init quiet loglevel=3 noapic"

