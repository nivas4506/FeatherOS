#!/usr/bin/env bash
#
# Create a bootable FeatherOS QEMU/Raw disk image
# Flow: Hardware -> Linux Kernel -> OS Layer -> Application Layer
#
set -euo pipefail

IMAGE_NAME="featheros-live.img"
IMAGE_SIZE_MB=512

echo "==> Creating empty disk image (${IMAGE_SIZE_MB}MB)..."
dd if=/dev/zero of="${IMAGE_NAME}" bs=1M count="${IMAGE_SIZE_MB}" status=progress

echo "==> Formatting image partition with ext4..."
mkfs.ext4 -F "${IMAGE_NAME}"

echo "==> Populating disk rootfs with FeatherOS structure..."
MOUNT_DIR=$(mktemp -d)
sudo mount -o loop "${IMAGE_NAME}" "${MOUNT_DIR}"

sudo mkdir -p "${MOUNT_DIR}"/{bin,sbin,etc,proc,sys,dev,tmp,run,boot/grub,os/bin,var/log,root/.feather}

# Install init bootstrap
sudo cp kernel/init/init "${MOUNT_DIR}/init"
sudo chmod +x "${MOUNT_DIR}/init"
sudo cp kernel/boot/grub.cfg "${MOUNT_DIR}/boot/grub/grub.cfg"

# Copy compiled binaries if present
if [ -f "target/release/featherd" ]; then
    sudo cp target/release/featherd "${MOUNT_DIR}/os/bin/featherd"
    sudo chmod +x "${MOUNT_DIR}/os/bin/featherd"
fi

if [ -f "target/release/aish" ]; then
    sudo cp target/release/aish "${MOUNT_DIR}/bin/feather"
    sudo chmod +x "${MOUNT_DIR}/bin/feather"
fi

sudo umount "${MOUNT_DIR}"
rm -rf "${MOUNT_DIR}"

echo "==> Bootable image created: ${IMAGE_NAME}"
echo "    Test with: qemu-system-x86_64 -m 1G -hda ${IMAGE_NAME} -nographic"
