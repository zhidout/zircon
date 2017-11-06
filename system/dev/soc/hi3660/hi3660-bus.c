// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

#include "pl061.h"
#include "hi3660-bus.h"
#include "hi3660-hw.h"

static pl061_gpios_t* find_gpio(hi3660_bus_t* bus, uint32_t index) {
    pl061_gpios_t* gpios;
    // TODO(voydanoff) consider using a fancier data structure here
    list_for_every_entry(&bus->gpios, gpios, pl061_gpios_t, node) {
        if (index >= gpios->gpio_start && index < gpios->gpio_start + gpios->gpio_count) {
            return gpios;
        }
    }
    zxlogf(ERROR, "find_gpio failed for index %u\n", index);
    return NULL;
}

static zx_status_t hi3660_gpio_config(void* ctx, uint32_t index, gpio_config_flags_t flags) {
    hi3660_bus_t* bus = ctx;
    pl061_gpios_t* gpios = find_gpio(bus, index);
    if (!gpios) {
        return ZX_ERR_INVALID_ARGS;
    }
    return pl061_proto_ops.config(gpios, index, flags);
}

static zx_status_t hi3660_gpio_read(void* ctx, uint32_t index, uint8_t* out_value) {
    hi3660_bus_t* bus = ctx;
    pl061_gpios_t* gpios = find_gpio(bus, index);
    if (!gpios) {
        return ZX_ERR_INVALID_ARGS;
    }
    return pl061_proto_ops.read(gpios, index, out_value);
}

static zx_status_t hi3660_gpio_write(void* ctx, uint32_t index, uint8_t value) {
    hi3660_bus_t* bus = ctx;
    pl061_gpios_t* gpios = find_gpio(bus, index);
    if (!gpios) {
        return ZX_ERR_INVALID_ARGS;
    }
    return pl061_proto_ops.write(gpios, index, value);
}

static gpio_protocol_ops_t gpio_ops = {
    .config = hi3660_gpio_config,
    .read = hi3660_gpio_read,
    .write = hi3660_gpio_write,
};

static void hi3660_release(void* ctx) {
    hi3660_bus_t* bus = ctx;
    pl061_gpios_t* gpios;

    while ((gpios = list_remove_head_type(&bus->gpios, pl061_gpios_t, node)) != NULL) {
        io_buffer_release(&gpios->buffer);
        free(gpios);
    }

    io_buffer_release(&bus->usb3otg_bc);
    io_buffer_release(&bus->peri_crg);
    io_buffer_release(&bus->pctrl);

    free(bus);
}

static zx_protocol_device_t hi3660_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = hi3660_release,
};

static zx_status_t hi3660_bind(void* ctx, zx_device_t* parent, void** cookie) {
printf("hi3660_bind\n");
    hi3660_bus_t* bus = calloc(1, sizeof(hi3660_bus_t));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &bus->pbus) != ZX_OK) {
        free(bus);
        return ZX_ERR_NOT_SUPPORTED;
    }

    list_initialize(&bus->gpios);

    // TODO(voydanoff) get from platform bus driver somehow
    zx_handle_t resource = get_root_resource();
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

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "hi3660-bus",
        .ctx = bus,
        .ops = &hi3660_device_protocol,
        // nothing should bind to this device
        // all interaction will be done via the pbus_interface_t
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, NULL);
    if (status != ZX_OK) {
        goto fail;
    }

    bus->gpio.ops = &gpio_ops;
    bus->gpio.ctx = bus;

    const pbus_protocol_t protocols[] = {
        {
            .proto_id = ZX_PROTOCOL_GPIO,
            .ctx = bus->gpio.ctx,
            .ops = bus->gpio.ops,
        },
    };

printf("hi3660_bind pbus_register_protocols\n");
    if ((status = pbus_register_protocols(&bus->pbus,  protocols, countof(protocols))) != ZX_OK) {
        zxlogf(ERROR, "hi3660_bind: pbus_register_protocols failed!\n");;
    }

    if ((status = hi3360_add_gpios(bus)) != ZX_OK) {
        zxlogf(ERROR, "hi3660_bind: hi3360_add_gpios failed!\n");;
    }

    if ((status = hi3360_add_devices(bus)) != ZX_OK) {
        zxlogf(ERROR, "hi3660_bind: hi3360_add_devices failed!\n");;
    }

    if ((status = hi3360_usb_init(bus)) != ZX_OK) {
        zxlogf(ERROR, "hi3660_bind: hi3360_usb_init failed!\n");;
    }

    return ZX_OK;

fail:
    zxlogf(ERROR, "hi3660_bind failed %d\n", status);
    hi3660_release(bus);
    return status;
}

static zx_driver_ops_t hi3660_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = hi3660_bind,
};

ZIRCON_DRIVER_BEGIN(hi3660, hi3660_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_BUS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_HI_SILICON),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_HI3660),
ZIRCON_DRIVER_END(hi3660)
