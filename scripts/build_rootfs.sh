#!/usr/bin/env bash
# FeatherOS Rootfs & Initramfs Assembler
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
ROOTFS_DIR="${BUILD_DIR}/rootfs"
USERSPACE_SRC="${ROOT_DIR}/src/userspace"
INITRAMFS_OUT="${BUILD_DIR}/initramfs.cpio.gz"

echo "=========================================================="
echo "         Building FeatherOS Root Filesystem (Rootfs)      "
echo "=========================================================="

# Ensure build directories exist
mkdir -p "${BUILD_DIR}"
rm -rf "${ROOTFS_DIR}"
mkdir -p "${ROOTFS_DIR}"

# 0. Check essential host packaging tools
if ! command -v cpio >/dev/null 2>&1; then
    echo " [*] cpio not found. Installing cpio..."
    sudo apt-get update && sudo apt-get install -y cpio
fi

# 1. Create standard UNIX filesystem hierarchy
echo " [*] Creating directory skeleton..."
mkdir -p "${ROOTFS_DIR}"/{bin,sbin,usr/bin,usr/sbin,proc,sys,dev,etc,root,home,tmp,run,var/log}
chmod 1777 "${ROOTFS_DIR}/tmp"

# 2. Acquire or install static BusyBox
BUSYBOX_BIN="${BUILD_DIR}/busybox-static"
if [ ! -f "${BUSYBOX_BIN}" ]; then
    echo " [*] Fetching static musl-based BusyBox binary..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox" -o "${BUSYBOX_BIN}"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox" -O "${BUSYBOX_BIN}"
    else
        echo " [!] Neither curl nor wget found. Attempting apt install of busybox-static..."
        sudo apt-get update && sudo apt-get install -y busybox-static
        cp /bin/busybox "${BUSYBOX_BIN}"
    fi
    chmod +x "${BUSYBOX_BIN}"
fi

# 3. Install BusyBox and symlink userland utilities
echo " [*] Installing BusyBox command suite..."
cp "${BUSYBOX_BIN}" "${ROOTFS_DIR}/bin/busybox"
chmod +x "${ROOTFS_DIR}/bin/busybox"

# Symlink all commands using relative links in /bin
for cmd in $("${BUSYBOX_BIN}" --list); do
    ln -sf "busybox" "${ROOTFS_DIR}/bin/${cmd}"
done

# Also symlink /sbin, /usr/bin, /usr/sbin to /bin/busybox
for dir in sbin usr/bin usr/sbin; do
    for cmd in $("${BUSYBOX_BIN}" --list); do
        ln -sf "/bin/busybox" "${ROOTFS_DIR}/${dir}/${cmd}"
    done
done

# 4. Install FeatherOS Init System (PID 1)
echo " [*] Installing FeatherOS /init sequence..."
cp "${USERSPACE_SRC}/init" "${ROOTFS_DIR}/init"
chmod +x "${ROOTFS_DIR}/init"

# 5. Install configuration and system metadata
echo " [*] Installing system configurations..."
if [ -d "${USERSPACE_SRC}/etc" ]; then
    cp -r "${USERSPACE_SRC}/etc/"* "${ROOTFS_DIR}/etc/"
fi

# Essential /etc files
echo "root:x:0:0:root:/root:/bin/sh" > "${ROOTFS_DIR}/etc/passwd"
echo "root:x:0:" > "${ROOTFS_DIR}/etc/group"
echo "featheros" > "${ROOTFS_DIR}/etc/hostname"
echo "127.0.0.1 localhost featheros" > "${ROOTFS_DIR}/etc/hosts"

# 6. Package into compressed CPIO archive (initramfs)
echo " [*] Compressing rootfs into ${INITRAMFS_OUT}..."
cd "${ROOTFS_DIR}"
find . -print0 | cpio --null -ov --format=newc | gzip -9 > "${INITRAMFS_OUT}"
cd "${ROOT_DIR}"

SIZE_KB=$(du -k "${INITRAMFS_OUT}" | cut -f1)
echo "=========================================================="
echo " [✓] FeatherOS Rootfs successfully built!"
echo " [✓] Output: ${INITRAMFS_OUT} (${SIZE_KB} KB)"
echo "=========================================================="

