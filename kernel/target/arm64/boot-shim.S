// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <asm.h>
#include <arch/asm_macros.h>

tmp             .req x16
device_tree     .req x19
bootdata        .req x20

#define STACK_SIZE  4096

.section .text
FUNCTION(_start)
    // x0 points to device tree at entry
    mov     device_tree, x0

    // compute beginning of bootdata
    adr     x1, _start
    add     x1, x1, #KERNEL_ALIGN
    mov     bootdata, x1

    // setup stack
    adr_global tmp, stack_end
    mov     sp, tmp

    // x0: pointer to device tree
    // x1: pointer to bootdata
    bl      boot_shim
    // kernel entry point is returned in x0

    mov     tmp, x0
    // pass device tree and bootdata to kernel in x0 and x1
    mov     x0, device_tree
    mov     x1, bootdata
    br      tmp
END_FUNCTION(_start)

.bss
LOCAL_DATA(stack)
    .skip 8192
    .balign 16
LOCAL_DATA(stack_end)
END_DATA(stack)
