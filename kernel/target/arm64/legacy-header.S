// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <arch/asm_macros.h>
#include <zircon/boot/bootdata.h>

// Register usage
bd_base     .req x9         // beginning of bootdata
bd_size     .req x10        // size of bootdata
kern_base   .req x11        // new location for kernel
src         .req x12        // source for copy
dest        .req x13        // dest for copy
count       .req x14        // counter for copy

// scratch registers
foo         .req x15
bar         .req x16
foo32       .req w15
bar32       .req w16

.section .text

_loader:
    adr     bd_base, _bootdata
//    mov     x0, bd_base             // leave pointer to boot data in x0 for the kernel

    // sanity check the bootdata headers
    ldr     foo32, [bd_base, 0]
    movlit  bar32, BOOTDATA_CONTAINER
    cmp     foo32, bar32
    bne     _fail
    ldr     foo32, [bd_base, 32]
    movlit  bar32, BOOTDATA_KERNEL
    cmp     foo32, bar32
    bne     _fail

    // compute total boot data size
    ldr     foo32, [bd_base, 4]     // read container length field
    ubfx    foo, foo, #0, #32       // 64 bit extend   
    add     bd_size, foo, #32       // add container header size


    // compute new kernel location
    add     kern_base, bd_base, bd_size
    // round up to 64K byte boundary
    movlit  foo, 0xffff
    add     kern_base, kern_base, foo
    and     kern_base, kern_base, #0xffffffffffff0000

    // copy kernel to new location
    mov     src, bd_base
    mov     dest, kern_base
    
 
    ldr     foo32, [bd_base, 32 + 4]    // read kernel size from kernel bootdata header
    ubfx    count, foo, #0, #32         // 64 bit extend   
    add     count, count, #64           // add size of the two bootdata headers
    add     count, count, #15           // round up to 16 byte boundary
    and     count, count, #0xfffffffffffffff0 

    // copy kernel image from src to dest
 _copy_loop:
    ldp     foo, bar, [src], #16
    stp     foo, bar, [dest], #16
    subs    count, count, #16
    bne     _copy_loop
    
    add     foo, kern_base, #64    // skip two bootdata records to get address of _start offset in x10
    ldr     foo, [foo]          // read the _start offset
    add     foo, foo, bd_base    // add it to beginning of bootdata
    br      foo                 // jump to it

_fail:
    b       _fail

    .balign 65536,0

_bootdata:
