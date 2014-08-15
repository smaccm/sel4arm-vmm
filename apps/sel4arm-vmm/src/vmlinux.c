/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "vmlinux.h"

#include <string.h>

#include <vka/capops.h>

#include <sel4arm-vmm/vm.h>
#include <sel4arm-vmm/images.h>
#include <sel4arm-vmm/exynos/devices.h>

#include <cpio/cpio.h>

#define LINUX_RAM_BASE    0x40000000
#define LINUX_RAM_SIZE    0x40000000
#define ATAGS_ADDR        (LINUX_RAM_BASE + 0x100)
#define DTB_ADDR          (LINUX_RAM_BASE + 0x09000000)

#define MACH_TYPE_EXYNOS5410 4151
#define MACH_TYPE_SPECIAL    ~0
#define MACH_TYPE            MACH_TYPE_SPECIAL

extern char _cpio_archive[];

extern vka_t _vka;
extern vspace_t _vspace;

static const struct device *linux_pt_devices[] = {
    &dev_ps_pwm_timer,
    &dev_ps_gpio_right,
    &dev_ps_alive,
    &dev_ps_cmu_top,
    &dev_ps_cmu_core,
    &dev_ps_chip_id,
    &dev_ps_cmu_cpu,
    &dev_ps_cmu_cdrex,
    &dev_ps_cmu_mem,
    &dev_ps_cmu_isp,
    &dev_ps_cmu_acp,
    &dev_ps_sysreg,
    &dev_i2c1,
    &dev_i2c2,
    &dev_i2c4,
    &dev_i2chdmi,
    &dev_usb2_ohci,
    &dev_usb2_ehci,
    &dev_usb2_ctrl,
    &dev_ps_msh0,
    &dev_ps_msh2,
    &dev_gpio_left,
    &dev_uart0,
    &dev_uart1,
    //&dev_uart1, /* Console */
    &dev_uart3,
    &dev_ps_tx_mixer,
    &dev_ps_hdmi0,
    &dev_ps_hdmi1,
    &dev_ps_hdmi2,
    &dev_ps_hdmi3,
    &dev_ps_hdmi4,
    &dev_ps_hdmi5,
    &dev_ps_hdmi6,
    &dev_ps_pdma0,
    &dev_ps_pdma1,
    &dev_ps_mdma0,
    &dev_ps_mdma1,
};

static int
install_linux_devices(vm_t* vm)
{
    int err;
    int i;
    /* Install virtual devices */
    err = vm_install_vgic(vm);
    assert(!err);
    err = vm_install_ram_range(vm, LINUX_RAM_BASE, LINUX_RAM_SIZE);
    assert(!err);
    err = vm_install_vcombiner(vm);
    assert(!err);
    err = vm_install_vmct(vm);
    assert(!err);

    err = vm_install_passthrough_device(vm, &dev_vconsole);
    assert(!err);

    /* Install pass through devices */
    for (i = 0; i < sizeof(linux_pt_devices) / sizeof(*linux_pt_devices); i++) {
        err = vm_install_passthrough_device(vm, linux_pt_devices[i]);
        assert(!err);
    }

    return 0;
}

static uint32_t
install_linux_dtb(vm_t* vm, const char* dtb_name)
{
    void* file;
    unsigned long size;
    uint32_t dtb_addr;

    /* Retrieve the file data */
    file = cpio_get_file(_cpio_archive, dtb_name, &size);
    if (file == NULL) {
        printf("Error: Linux dtb file \'%s\' not found\n", dtb_name);
        return 0;
    }
    if (image_get_type(file) != IMG_DTB) {
        printf("Error: \'%s\' is not a device tree\n", dtb_name);
        return 0;
    }

    /* Copy the tree to the VM */
    dtb_addr = DTB_ADDR;
    if (vm_copyout(vm, file, dtb_addr, size)) {
        printf("Error: Failed to load device tree \'%s\'\n", dtb_name);
        return 0;
    } else {
        return dtb_addr;
    }
}

static void*
install_linux_kernel(vm_t* vm, const char* kernel_name)
{
    void* file;
    unsigned long size;
    uintptr_t entry;

    /* Retrieve the file data */
    file = cpio_get_file(_cpio_archive, kernel_name, &size);
    if (file == NULL) {
        printf("Error: Unable to find kernel image \'%s\'\n", kernel_name);
        return NULL;
    }

    /* Determine the load address */
    switch (image_get_type(file)) {
    case IMG_BIN:
        entry = LINUX_RAM_BASE + 0x8000;
        break;
    case IMG_ZIMAGE:
        entry = zImage_get_load_address(file, LINUX_RAM_BASE);
        break;
    default:
        printf("Error: Unknown Linux image format for \'%s\'\n", kernel_name);
        return NULL;
    }

    /* Load the image */
    if (vm_copyout(vm, file, entry, size)) {
        printf("Error: Failed to load \'%s\'\n", kernel_name);
        return NULL;
    } else {
        return (void*)entry;
    }
}

int
load_linux(vm_t* vm, const char* kernel_name, const char* dtb_name)
{
    void* entry;
    uint32_t dtb;
    int err;

    /* Install devices */
    err = install_linux_devices(vm);
    if (err) {
        printf("Error: Failed to install Linux devices\n");
        return -1;
    }
    /* Load kernel */
    entry = install_linux_kernel(vm, kernel_name);
    if (!entry) {
        return -1;
    }
    /* Load device tree */
    dtb = install_linux_dtb(vm, dtb_name);
    if (!dtb) {
        return -1;
    }
    /* Set boot arguments */
    err = vm_set_bootargs(vm, entry, MACH_TYPE, dtb);
    if (err) {
        printf("Error: Failed to set boot arguments\n");
        return -1;
    }

    return 0;
}

