#!/usr/bin/env bash
# FeatherOS Kernel Build & Config Script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
KERNEL_SRC="${ROOT_DIR}/src/kernel"
BUILD_DIR="${ROOT_DIR}/build"
CONFIG_FILE="${ROOT_DIR}/configs/kernel-featheros.config"
BZIMAGE_OUT="${BUILD_DIR}/bzImage"

MODE="${1:-build}"
NPROC=$(nproc 2>/dev/null || echo 4)

echo "=========================================================="
echo "            FeatherOS Linux Kernel Build System           "
echo "=========================================================="

mkdir -p "${BUILD_DIR}"
cd "${KERNEL_SRC}"

# 1. Check essential host build tools
check_dependencies() {
    local missing=()
    for tool in gcc make bison flex bc; do
        if ! command -v "${tool}" >/dev/null 2>&1; then
            missing+=("${tool}")
        fi
    done
    if [ ${#missing[@]} -ne 0 ]; then
        echo " [!] Missing build dependencies: ${missing[*]}"
        echo " [*] Installing build tools via apt..."
        sudo apt-get update && sudo apt-get install -y build-essential libncurses-dev bison flex libssl-dev libelf-dev bc
    fi
}

configure_kernel() {
    echo " [*] Applying baseline x86_64 configuration..."
    make x86_64_defconfig

    if [ -f "${CONFIG_FILE}" ]; then
        echo " [*] Merging FeatherOS optimized configuration fragment..."
        if [ -x "./scripts/kconfig/merge_config.sh" ]; then
            ./scripts/kconfig/merge_config.sh -m .config "${CONFIG_FILE}"
            make olddefconfig
        else
            cat "${CONFIG_FILE}" >> .config
            make olddefconfig
        fi
    fi
}

case "${MODE}" in
    defconfig)
        check_dependencies
        configure_kernel
        echo " [✓] Kernel configured for FeatherOS."
        ;;
    menuconfig)
        check_dependencies
        if [ ! -f .config ]; then
            configure_kernel
        fi
        make menuconfig
        ;;
    build)
        check_dependencies
        if [ ! -f .config ]; then
            configure_kernel
        fi
        echo " [*] Compiling FeatherOS kernel using ${NPROC} parallel jobs..."
        make -j"${NPROC}" bzImage
        
        if [ -f "arch/x86/boot/bzImage" ]; then
            cp "arch/x86/boot/bzImage" "${BZIMAGE_OUT}"
            SIZE_MB=$(du -m "${BZIMAGE_OUT}" | cut -f1)
            echo "=========================================================="
            echo " [✓] Kernel successfully built!"
            echo " [✓] Output: ${BZIMAGE_OUT} (${SIZE_MB} MB)"
            echo "=========================================================="
        else
            echo " [✗] Error: bzImage not found after compilation."
            exit 1
        fi
        ;;
    *)
        echo "Usage: $0 [defconfig|menuconfig|build]"
        exit 1
        ;;
esac

