// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <zircon/boot/bootdata.h>

uint64_t boot_shim(void* device_tree, bootdata_t* bootdata);

static void fail(void) {
    while (1) {}
}

uint64_t boot_shim(void* device_tree, bootdata_t* bootdata) {
    // sanity check the bootdata headers
    // it must start with a container record followed by a kernel record
    if (bootdata[0].type != BOOTDATA_CONTAINER || bootdata[0].extra != BOOTDATA_MAGIC ||
        bootdata[0].magic != BOOTITEM_MAGIC || bootdata[1].type != BOOTDATA_KERNEL ||
        bootdata[1].magic != BOOTITEM_MAGIC) {
        fail();
    }

    // kernel rec follows kernel bootdata header
    bootdata_kernel_t* kernel_rec = (bootdata_kernel_t *)&bootdata[2];

    void* kernel_base;
    uint32_t bootdata_size = bootdata[0].length + sizeof(bootdata_t);
    uint32_t kernel_size = bootdata[1].length + 2 * sizeof(bootdata_t);

    if (bootdata_size > kernel_size) {
        // we have more bootdata following the kernel.
        // we must relocate the kernel after the rest of the bootdata.

        // round up to align new kernel location
        bootdata_size = ((bootdata_size + (KERNEL_ALIGN - 1)) / KERNEL_ALIGN) * KERNEL_ALIGN;
        kernel_base = bootdata + bootdata_size;

        // poor-man's memcpy, since we don't have a libc in here
        uint64_t* src = (uint64_t *)bootdata;
        uint64_t* dest = (uint64_t *)kernel_base;
        uint64_t* end = (uint64_t *)((void *)bootdata + kernel_size);
        end = (uint64_t *)(((uint64_t)end + 7) & ~7);

        while (src < end) {
            *dest++ = *src++;
        }
    } else {
        kernel_base = bootdata;
    }

    // return kernel entry point address
    return (uint64_t)kernel_base + kernel_rec->entry64;
}