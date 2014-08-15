/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <syscall_stubs_sel4.h>

#include <twinkle/allocator.h>
#include <twinkle/bootstrap.h>
#include <twinkle/vka.h>
#include <vka/capops.h>
#include <vka/object.h>

#include <vspace/vspace.h>
#include <simple/simple.h>
#include <simple-default/simple-default.h>
#include <platsupport/io.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/io.h>

#include <cpio/cpio.h>

#include <sel4arm-vmm/vm.h>

#include "vmlinux.h"

#define VM_PRIO             100
#define VM_BADGE            (1U << 0)
#define VM_LINUX_NAME       "linux"
#define VM_LINUX_DTB_NAME   "linux-dtb"
#define VM_NAME             "Linux"

MUSLC_SYSCALL_TABLE;

vka_t _vka;
simple_t _simple;
vspace_t _vspace;
sel4utils_alloc_data_t _alloc_data;
struct allocator *_allocator;
seL4_CPtr _fault_endpoint;

struct ps_io_ops _io_ops;

extern char _cpio_archive[];


static void
print_cpio_info(void)
{
    struct cpio_info info;
    const char* name;
    unsigned long size;
    int i;

    cpio_info(_cpio_archive, &info);

    printf("CPIO: %d files found.\n", info.file_count);
    assert(info.file_count > 0);
    for (i = 0; i < info.file_count; i++) {
        void * addr;
        char buf[info.max_path_sz + 1];
        buf[info.max_path_sz] = '\0';
        addr = cpio_get_entry(_cpio_archive, i, &name, &size);
        assert(addr);
        strncpy(buf, name, info.max_path_sz);
        printf("%d) %-20s  0x%08x, %8ld bytes\n", i, buf, (uint32_t)addr, size);
    }
    printf("\n");
}

static void
print_boot_info(void)
{
    seL4_BootInfo* bi;
    seL4_DeviceRegion* dr;
    int n_ut;
    int n_dr;
    int i;

    bi = seL4_GetBootInfo();

    /* Untyped */
    n_ut = bi->untyped.end - bi->untyped.start;
    assert(bi);
    printf("\n");
    printf("-------------------------------\n");
    printf("|  Boot info untyped regions  |\n");
    printf("-------------------------------\n");
    for (i = 0; i < n_ut; i++) {
        uint32_t start, end;
        int bits;
        start = bi->untypedPaddrList[i];
        bits = bi->untypedSizeBitsList[i];
        end = start + (1U << bits);
        printf("| 0x%08x->0x%08x (%2d) |\n", start, end, bits);
    }
    printf("-------------------------------\n\n");

    /* Device caps */
    printf("-------------------------------\n");
    printf("|  Boot info device regions   |\n");
    printf("-------------------------------\n");
    n_dr = bi->numDeviceRegions;
    dr = bi->deviceRegions;
    for (i = 0; i < n_dr; i++) {
        uint32_t start, end;
        int bits;
        start = dr[i].basePaddr;
        bits = dr[i].frameSizeBits;;
        end = start + ((1U << bits) * (dr[i].frames.end - dr[i].frames.start));
        printf("| 0x%08x->0x%08x (%2d) |\n", start, end, bits);
    }
    printf("-------------------------------\n\n");
}


static int
vmm_init(void)
{
    vka_object_t fault_ep_obj;
    vka_t* vka;
    simple_t* simple;
    vspace_t* vspace;
    int err;

    vka = &_vka;
    vspace = &_vspace;
    simple = &_simple;
    fault_ep_obj.cptr = 0;

    _allocator = create_first_stage_allocator();
    twinkle_init_vka(&_vka, _allocator);
    simple_default_init_bootinfo(simple, seL4_GetBootInfo());
    err = sel4utils_bootstrap_vspace_with_bootinfo_leaky(vspace,
                                                         &_alloc_data,
                                                         seL4_CapInitThreadPD,
                                                         vka,
                                                         seL4_GetBootInfo());
    assert(!err);

    /* Initialise device support */
    err = sel4platsupport_new_io_mapper(*simple, *vspace, *vka,
                                        &_io_ops.io_mapper);
    assert(!err);

    /* Setup debug port: printf() is only reliable after this */
    platsupport_serial_setup_simple(NULL, simple, vka);

    /* Allocate a endpoint for listening to events */
    err = vka_alloc_endpoint(vka, &fault_ep_obj);
    assert(!err);
    _fault_endpoint = fault_ep_obj.cptr;

    return 0;
}

int
main(void)
{
    struct vm vm;
    int err;

    SET_MUSLC_SYSCALL_TABLE;

    err = vmm_init();
    assert(!err);

    print_boot_info();
    print_cpio_info();

    /* Create the VM */
    err = vm_create(VM_NAME, VM_PRIO, _fault_endpoint, VM_BADGE,
                    &_vka, &_simple, &_vspace, &_io_ops, &vm);
    if (err) {
        printf("Failed to create VM\n");
        seL4_DebugHalt();
        return -1;
    }

    /* Load system images */
    printf("Loading Linux: \'%s\' dtb: \'%s\'\n", VM_LINUX_NAME, VM_LINUX_DTB_NAME);
    err = load_linux(&vm, VM_LINUX_NAME, VM_LINUX_DTB_NAME);
    if (err) {
        printf("Failed to load VM image\n");
        seL4_DebugHalt();
        return -1;
    }

    /* Power on */
    printf("Starting VM\n\n");
    err = vm_start(&vm);
    if (err) {
        printf("Failed to start VM\n");
        seL4_DebugHalt();
        return -1;
    }

    /* Loop forever, handling events */
    while (1) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge;

        tag = seL4_Wait(_fault_endpoint, &sender_badge);
        assert(sender_badge == VM_BADGE);

        err = vm_event(&vm, tag);
        if (err) {
            /* Shutdown */
            vm_stop(&vm);
            seL4_DebugHalt();
        }
    }

    return 0;
}

