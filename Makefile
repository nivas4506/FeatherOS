# FeatherOS Top-Level Orchestrator Makefile

KERNEL_SRC := src/kernel
BUILD_DIR  := build
CONFIG_DIR := configs
SCRIPTS_DIR:= scripts

KERNEL_CONFIG := $(CONFIG_DIR)/kernel-featheros.config
BZIMAGE       := $(BUILD_DIR)/bzImage
INITRAMFS     := $(BUILD_DIR)/initramfs.cpio.gz

.PHONY: all help kernel-defconfig kernel-menuconfig kernel rootfs run clean

all: help

help:
	@echo "=========================================================="
	@echo "                     FeatherOS Build System               "
	@echo "=========================================================="
	@echo "Available targets:"
	@echo "  make kernel-defconfig   - Apply baseline minimal FeatherOS config"
	@echo "  make kernel-menuconfig  - Open interactive kernel menuconfig"
	@echo "  make kernel             - Compile Linux kernel (outputs to build/bzImage)"
	@echo "  make rootfs             - Assemble FeatherOS initramfs with BusyBox & /init"
	@echo "  make run                - Launch FeatherOS in QEMU virtual machine"
	@echo "  make clean              - Clean build artifacts"
	@echo "=========================================================="

kernel-defconfig:
	@bash $(SCRIPTS_DIR)/build_kernel.sh defconfig

kernel-menuconfig:
	@bash $(SCRIPTS_DIR)/build_kernel.sh menuconfig

kernel:
	@bash $(SCRIPTS_DIR)/build_kernel.sh build

rootfs:
	@bash $(SCRIPTS_DIR)/build_rootfs.sh

run:
	@bash $(SCRIPTS_DIR)/run_qemu.sh

clean:
	@rm -rf $(BUILD_DIR)/*
	@touch $(BUILD_DIR)/.gitkeep
	@echo "Build directory cleaned."
