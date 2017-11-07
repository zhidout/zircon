// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>

__BEGIN_CDECLS

// The root resource
#define ZX_RSRC_KIND_ROOT        (0x0000u)  // allows access to everything

// Hardware resources
#define ZX_RSRC_KIND_MMIO        (0x1000u)  // allows access to zx_vmo_create_physical
#define ZX_RSRC_KIND_IOPORT      (0x1001u)  // unused
#define ZX_RSRC_KIND_IRQ         (0x1002u)  // allows access to zx_interrupt_create
#define ZX_RSRC_KIND_DEVICE_IO   (0x1003u)  // allows access to zx_mmap_device_io

// Subsystem resources
#define ZX_RSRC_KIND_HYPERVISOR  (0x2000u)  // allows access to zx_guest_create

// DDK resources
#define ZX_RSRC_ALLOC_CONTIG_VMO (0x3000u)  // allows access to zx_vmo_create_contiguous
#define ZX_RSRC_SET_FRAMEBUFFER  (0x3001u)  // allows access to zx_set_framebuffer and
                                            // zx_set_framebuffer_vmo
#define ZX_RSRC_ACPI_UEFI_RDSP   (0x3002u)  // allows access to zx_acpi_uefi_rsdp

__END_CDECLS
