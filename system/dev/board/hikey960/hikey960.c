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

#include <soc/hi3660/hi3660-bus.h>
#include <soc/hi3660/hi3660-hw.h>

#include "hikey960.h"

static zx_status_t hikey960_get_initial_mode(void* ctx, usb_mode_t* out_mode) {
    *out_mode = USB_MODE_HOST;
    return ZX_OK;
}

static zx_status_t hikey960_set_mode(void* ctx, usb_mode_t mode) {
    hikey960_t* bus = ctx;

    if (mode == USB_MODE_OTG) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    return hikey960_usb_set_mode(bus, mode);
}

usb_mode_switch_protocol_ops_t usb_mode_switch_ops = {
    .get_initial_mode = hikey960_get_initial_mode,
    .set_mode = hikey960_set_mode,
};

static zx_status_t hikey960_get_protocol(void* ctx, uint32_t proto_id, void* out) {
    hikey960_t* bus = ctx;

    switch (proto_id) {
    case ZX_PROTOCOL_GPIO: {
        memcpy(out, &bus->hi3660->gpio, sizeof(bus->hi3660->gpio));
        return ZX_OK;
    }
    case ZX_PROTOCOL_USB_MODE_SWITCH: {
        memcpy(out, &bus->usb_mode_switch, sizeof(bus->usb_mode_switch));
        return ZX_OK;
    }
    default:
        return ZX_ERR_NOT_SUPPORTED;
    }
}

static pbus_interface_ops_t hikey960_bus_ops = {
    .get_protocol = hikey960_get_protocol,
};

static void hikey960_release(void* ctx) {
    hikey960_t* bus = ctx;

    if (bus->hi3660) {
        hi3660_release(bus->hi3660);
    }
    free(bus);
}

static zx_protocol_device_t hikey960_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = hikey960_release,
};

static zx_status_t hikey960_bind(void* ctx, zx_device_t* parent) {
    hikey960_t* bus = calloc(1, sizeof(hikey960_t));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }

    if (device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &bus->pbus) != ZX_OK) {
        free(bus);
        return ZX_ERR_NOT_SUPPORTED;
    }
    bus->usb_mode = USB_MODE_NONE;

    // TODO(voydanoff) get from platform bus driver somehow
    zx_handle_t resource = get_root_resource();
    zx_status_t status = hi3660_init(resource, &bus->hi3660);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hikey960_bind: hi3660_init failed %d\n", status);
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "hikey960",
        .ctx = bus,
        .ops = &hikey960_device_protocol,
        // nothing should bind to this device
        // all interaction will be done via the pbus_interface_t
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, NULL);
    if (status != ZX_OK) {
        goto fail;
    }

    bus->usb_mode_switch.ops = &usb_mode_switch_ops;
    bus->usb_mode_switch.ctx = bus;

    pbus_interface_t intf;
    intf.ops = &hikey960_bus_ops;
    intf.ctx = bus;
    pbus_set_interface(&bus->pbus, &intf);

    if ((status = hikey960_add_devices(bus)) != ZX_OK) {
        zxlogf(ERROR, "hikey960_bind: hi3660_add_devices failed!\n");;
    }

    hikey960_usb_set_mode(bus, USB_MODE_NONE);

    return ZX_OK;

fail:
    zxlogf(ERROR, "hikey960_bind failed %d\n", status);
    hikey960_release(bus);
    return status;
}

static zx_driver_ops_t hikey960_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = hikey960_bind,
};

ZIRCON_DRIVER_BEGIN(hikey960, hikey960_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_BUS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_96BOARDS),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_HIKEY960),
ZIRCON_DRIVER_END(hikey960)
