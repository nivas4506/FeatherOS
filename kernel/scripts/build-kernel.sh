#!/usr/bin/env bash
#
# Build script for FeatherOS Linux Kernel Base Layer
#
set -euo pipefail

KERNEL_VERSION="${KERNEL_VERSION:-6.6.30}"
KERNEL_DIR="build/linux-${KERNEL_VERSION}"
KERNEL_TAR="linux-${KERNEL_VERSION}.tar.xz"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/${KERNEL_TAR}"

echo "==> [FeatherOS Kernel] Preparing Linux Kernel ${KERNEL_VERSION} Base Layer..."
mkdir -p build

if [ ! -f "build/${KERNEL_TAR}" ]; then
    echo "==> Downloading upstream Linux kernel source..."
    curl -L "${KERNEL_URL}" -o "build/${KERNEL_TAR}"
fi

if [ ! -d "${KERNEL_DIR}" ]; then
    echo "==> Extracting kernel source..."
    tar -xf "build/${KERNEL_TAR}" -C build/
fi

echo "==> Applying FeatherOS defconfig..."
cp kernel/config/feather_defconfig "${KERNEL_DIR}/.config"

echo "==> Building kernel bzImage and modules..."
cd "${KERNEL_DIR}"
make -j"$(nproc)" bzImage modules

echo "==> Kernel build completed successfully!"
echo "    Artifact: ${KERNEL_DIR}/arch/x86/boot/bzImage"
