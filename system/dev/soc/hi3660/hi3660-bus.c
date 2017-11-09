// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/driver.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>
#include <soc/hi3660/hi3660-bus.h>
#include <soc/hi3660/hi3660-regs.h>
#include <soc/hi3660/hi3660-hw.h>

#include <stdlib.h>

zx_status_t hi3660_init(zx_handle_t resource, hi3660_bus_t** out_bus) {
    hi3660_bus_t* bus = calloc(1, sizeof(hi3660_bus_t));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }
    list_initialize(&bus->gpios);

    zx_status_t status;
    if ((status = io_buffer_init_physical(&bus->usb3otg_bc, MMIO_USB3OTG_BC_BASE,
                                          MMIO_USB3OTG_BC_LENGTH, resource,
                                          ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK ||
         (status = io_buffer_init_physical(&bus->peri_crg, MMIO_PERI_CRG_BASE, MMIO_PERI_CRG_LENGTH,
                                           resource, ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK ||
         (status = io_buffer_init_physical(&bus->pctrl, MMIO_PCTRL_BASE, MMIO_PCTRL_LENGTH,
                                           resource, ZX_CACHE_POLICY_UNCACHED_DEVICE)) != ZX_OK) {
        goto fail;
    }

    status = hi3660_gpio_init(bus);
    if (status != ZX_OK) {
        goto fail;
    }
    status = hi3660_usb_init(bus);
    if (status != ZX_OK) {
        goto fail;
    }

    *out_bus = bus;
    return ZX_OK;

fail:
    zxlogf(ERROR, "hi3660_init failed %d\n", status);
    hi3660_release(bus);
    return status;
}

void hi3660_release(hi3660_bus_t* bus) {
    hi3660_gpio_release(bus);

    io_buffer_release(&bus->usb3otg_bc);
    io_buffer_release(&bus->peri_crg);
    io_buffer_release(&bus->pctrl);
    free(bus);
}