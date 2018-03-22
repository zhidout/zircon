// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

//#include <asm.h>

.section .text

_start:    
    adr     x9, _end        // beginning of kernel image in x9
    add     x10, x9, #64    // skip two bootdata records to get address of _start offset in x10
    ldr     x10, [x10]      // read the _start offset
    add     x10, x10, x9    // add it to beginning of kernel
    br      x10             // jump to +start

    .balign 65536,0

_end:
