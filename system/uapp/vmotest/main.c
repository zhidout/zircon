// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>

#include <zircon/types.h>
#include <zircon/syscalls.h>
#include <zircon/process.h>

#define VMO_LEN 4096

int main(int argc, char **argv) {
    zx_status_t st;

    zx_handle_t vmo;
    st = zx_vmo_create(VMO_LEN, 0, &vmo);
    printf("zx_vmo_create returned %d\n", st);
    if (st != ZX_OK) {
        printf("aborting!\n");
        return -1;
    }

    // Map the vmo into to root aspace.
    uintptr_t ptr;
    st = zx_vmar_map(zx_vmar_root_self(), 0, vmo, 0, VMO_LEN, ZX_VM_FLAG_PERM_READ, &ptr);
    printf("zx_vmar_map returned %d\n", st);
    if (st != ZX_OK) {
        printf("aborting!\n");
        return -1;
    }

    st = zx_cache_flush((void*)ptr, VMO_LEN, ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INVALIDATE);
    printf("zx_cache_flush returned %d\n", st);
    if (st != ZX_OK) {
        printf("aborting!\n");
        return -1;
    }

    return 0;
}
