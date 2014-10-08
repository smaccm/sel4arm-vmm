#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# Targets
TARGETS := linux linux-dtb

CONFIG     := odroidxu_sel4vm_defconfig
LINUX_FILE := Image
ifeq ($(CONFIG_PLAT_EXYNOS5250),y)
DTB_FILE   := exynos5250-arndale-sel4vm.dtb
else ifeq ($(CONFIG_PLAT_EXYNOS5410),y)
DTB_FILE   := exynos5410-odroidxu-sel4vm.dtb
else
$(error Unknown platform)
endif

LINUX_DIR=$(SOURCE_DIR)/linux


-include $(SEL4_COMMON)/common.mk

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabi-
unexport CFLAGS
unexport LDFLAGS
unexport MAKEFLAGS

$(BUILD_DIR)/.config:
	@echo "Configuring Linux..."
	$(MAKE) -C $(LINUX_DIR) O=$(BUILD_DIR) $(CONFIG)

$(BUILD_DIR)/arch/arm/boot/$(LINUX_FILE) $(BUILD_DIR)/arch/arm/boot/dts/$(DTB_FILE): $(BUILD_DIR)/.config .FORCE
	@echo "Building Linux..."
	$(MAKE) -C $(LINUX_DIR) O=$(BUILD_DIR) -j8

$(SEL4_BINDIR)/linux: $(BUILD_DIR)/arch/arm/boot/$(LINUX_FILE)
	$(Q)mkdir -p $(dir $@)
	$(call cp_file,$<,$@)

$(SEL4_BINDIR)/linux-dtb: $(BUILD_DIR)/arch/arm/boot/dts/$(DTB_FILE)
	$(Q)mkdir -p $(dir $@)
	$(call cp_file,$<,$@)


linux: $(SEL4_BINDIR)/linux
linux-dtb: $(SEL4_BINDIR)/linux-dtb

.PHONY: linux linux-dtb

.FORCE:


