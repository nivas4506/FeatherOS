#!/usr/bin/env bash
# FeatherOS Wayland Graphical Desktop Rootfs Assembler
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DESKTOP_STAGE="/tmp/featheros_desktop_rootfs"
INITRAMFS_OUT="${BUILD_DIR}/desktop_initramfs.cpio.gz"

echo "=========================================================="
echo "      Building FeatherOS Wayland Desktop Rootfs           "
echo "=========================================================="

mkdir -p "${BUILD_DIR}"
rm -rf "${DESKTOP_STAGE}"
mkdir -p "${DESKTOP_STAGE}"

# 1. Download Alpine minirootfs base (lightweight Musl environment)
echo " [*] Downloading Alpine minirootfs base..."
cd /tmp
TARBALL="alpine-minirootfs-3.21.3-x86_64.tar.gz"
if [ ! -f "/tmp/${TARBALL}" ]; then
    curl -fsSL "https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/${TARBALL}" -o "/tmp/${TARBALL}" || \
    curl -fsSL "https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz" -o "/tmp/${TARBALL}"
fi

echo " [*] Extracting base system into ${DESKTOP_STAGE}..."
tar -xzf "/tmp/${TARBALL}" -C "${DESKTOP_STAGE}"

# 2. Setup DNS and Repositories for Chroot
mkdir -p "${DESKTOP_STAGE}/etc/apk"
echo "nameserver 1.1.1.1" > "${DESKTOP_STAGE}/etc/resolv.conf"
cat << 'EOF' > "${DESKTOP_STAGE}/etc/apk/repositories"
https://dl-cdn.alpinelinux.org/alpine/v3.21/main
https://dl-cdn.alpinelinux.org/alpine/v3.21/community
EOF

# 3. Chroot and install Weston Wayland desktop packages
echo " [*] Installing Weston, Wayland, and GPU drivers..."
chroot "${DESKTOP_STAGE}" /bin/sh -c "
apk update
apk add --no-cache weston weston-terminal weston-shell-desktop mesa-dri-gallium udev dbus font-dejavu shadow
"

# 4. Configure Weston desktop shell & FeatherOS theme
mkdir -p "${DESKTOP_STAGE}/etc/xdg/weston"
cat << 'EOF' > "${DESKTOP_STAGE}/etc/xdg/weston/weston.ini"
[core]
backend=drm-backend.so
shell=desktop-shell.so
idle-time=0

[shell]
panel-position=top
panel-color=0xdd1a1a24
clock-format=24h
background-color=0xff0f0f17
locking=false
animation=zoom

[launcher]
icon=/usr/share/weston/icon_terminal.png
path=/usr/bin/weston-terminal
EOF

# 5. Create Desktop /init
cat << 'EOF' > "${DESKTOP_STAGE}/init"
#!/bin/sh
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export HOME=/root
export XDG_RUNTIME_DIR=/run/user/0

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts /dev/shm /run/user/0 /tmp
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /dev/shm
mount -t tmpfs tmpfs /run
mount -t tmpfs tmpfs /tmp
chmod 0700 /run/user/0

# Start udev for input/display handling
/sbin/udevd --daemon 2>/dev/null || true
udevadm trigger 2>/dev/null || true

hostname featheros

echo "=========================================================="
echo "        Starting FeatherOS Wayland Desktop GUI...        "
echo "=========================================================="

exec /usr/bin/weston --backend=drm-backend.so --log=/var/log/weston.log
EOF
chmod +x "${DESKTOP_STAGE}/init"

# 6. Compress rootfs into initramfs
echo " [*] Compressing desktop rootfs into ${INITRAMFS_OUT}..."
cd "${DESKTOP_STAGE}"
find . -print0 | cpio --null -ov --format=newc 2>/dev/null | gzip -1 > "${INITRAMFS_OUT}"
cd "${ROOT_DIR}"

SIZE_MB=$(du -m "${INITRAMFS_OUT}" | cut -f1)
echo "=========================================================="
echo " [✓] FeatherOS Desktop Rootfs successfully built!"
echo " [✓] Output: ${INITRAMFS_OUT} (${SIZE_MB} MB)"
echo "=========================================================="
