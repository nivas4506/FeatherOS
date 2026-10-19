#!/usr/bin/env bash
#
# Install script for FeatherOS Linux Kernel Base Layer
# Installs kernel bzImage, modules, and sets up bootloader configuration
#
set -euo pipefail

DESTDIR="${DESTDIR:-/}"
BOOT_DIR="${DESTDIR}/boot"
KERNEL_IMAGE="${1:-build/linux-6.6.30/arch/x86/boot/bzImage}"

if [ ! -f "${KERNEL_IMAGE}" ]; then
    echo "Error: Kernel image '${KERNEL_IMAGE}' not found. Run kernel/scripts/build-kernel.sh first."
    exit 1
fi

echo "==> [FeatherOS Installer] Installing Linux Kernel Base Layer to ${BOOT_DIR}..."
mkdir -p "${BOOT_DIR}"
mkdir -p "${BOOT_DIR}/grub"

# 1. Copy Kernel
cp "${KERNEL_IMAGE}" "${BOOT_DIR}/vmlinuz-feather"
echo "    Installed /boot/vmlinuz-feather"

# 2. Install GRUB Configuration
cp kernel/boot/grub.cfg "${BOOT_DIR}/grub/grub.cfg"
echo "    Installed /boot/grub/grub.cfg"

# 3. Assemble and install Initramfs
if [ -f "build/initramfs-feather.img" ]; then
    cp "build/initramfs-feather.img" "${BOOT_DIR}/initramfs-feather.img"
    echo "    Installed /boot/initramfs-feather.img"
fi

echo "==> Linux Kernel Base Layer installed successfully."
