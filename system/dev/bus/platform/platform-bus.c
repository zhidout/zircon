// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/platform-defs.h>
#include <zircon/process.h>

#include "platform-bus.h"

zx_status_t platform_bus_register_protocols(void* ctx, const pbus_protocol_t* protocols,
                                            size_t count) {
    platform_bus_t* bus = ctx;

    for (size_t i = 0; i < count; i++) {
        const pbus_protocol_t* proto = &protocols[i];
        switch (proto->proto_id) {
        case ZX_PROTOCOL_USB_MODE_SWITCH:
            bus->ums.ctx = proto->ctx;
            bus->ums.ops = proto->ops;
            break;
        case ZX_PROTOCOL_GPIO:
            bus->gpio.ctx = proto->ctx;
            bus->gpio.ops = proto->ops;
            break;
        case ZX_PROTOCOL_I2C:
            bus->i2c.ctx = proto->ctx;
            bus->i2c.ops = proto->ops;
            break;
        default:
            zxlogf(INFO, "unsupported protocol %08X in platform_bus_set_protocols\n", proto->proto_id);
            break;
        }
    }

    // signal that the platform bus driver has registered its protocols
    completion_signal(&bus->register_protocols_completion);

    return ZX_OK;
}

static zx_status_t platform_bus_device_add(void* ctx, const pbus_dev_t* dev, uint32_t flags) {
    platform_bus_t* bus = ctx;
printf("platform_bus_device_add %p\n", bus); 
    return platform_device_add(bus, dev, flags);
}

static zx_status_t platform_bus_device_enable(void* ctx, uint32_t vid, uint32_t pid, uint32_t did,
                                              bool enable) {
    platform_bus_t* bus = ctx;
    platform_dev_t* dev;
    list_for_every_entry(&bus->devices, dev, platform_dev_t, node) {
        if (dev->vid == vid && dev->pid == pid && dev->did == did) {
            return platform_device_enable(dev, enable);
        }
    }

    return ZX_ERR_NOT_FOUND;
}

static zx_protocol_device_t empty_bus_proto = {
    .version = DEVICE_OPS_VERSION,
};

static zx_status_t platform_bus_bus_device_add(void* ctx, uint32_t vid, uint32_t pid, uint32_t did);

static const char* platform_bus_get_board_name(void* ctx) {
    platform_bus_t* bus = ctx;
    return bus->board_name;
}

static platform_bus_protocol_ops_t platform_bus_proto_ops = {
    .register_protocols = platform_bus_register_protocols,
    .device_add = platform_bus_device_add,
    .device_enable = platform_bus_device_enable,
    .bus_device_add = platform_bus_bus_device_add,
    .get_board_name = platform_bus_get_board_name,
};

static zx_status_t platform_bus_bus_device_add(void* ctx, uint32_t vid, uint32_t pid,
                                               uint32_t did) {
    platform_bus_t* bus = ctx;

    zx_device_prop_t props[] = {
        {BIND_PLATFORM_DEV_VID, 0, vid},
        {BIND_PLATFORM_DEV_PID, 0, pid},
        {BIND_PLATFORM_DEV_DID, 0, pid},
    };

    char namestr[ZX_DEVICE_NAME_MAX];
    snprintf(namestr, sizeof(namestr), "%04x:%04x:%04x", vid, pid, did);

    device_add_args_t add_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = namestr,
        .ctx = bus, //????
        .ops = &empty_bus_proto,
        .proto_id = ZX_PROTOCOL_PLATFORM_BUS,
        .proto_ops = &platform_bus_proto_ops,
        .props = props,
        .prop_count = countof(props),
    };

    completion_reset(&bus->register_protocols_completion);
    zx_status_t status = device_add(bus->zxdev, &add_args, &bus->zxdev);
    if (status != ZX_OK) {
        return status;
    }
    status = completion_wait(&bus->register_protocols_completion, ZX_SEC(5));
    if (status != ZX_OK) {
        zxlogf(ERROR, "pbus_bus_device_add: platform bus driver didn't "
                      "call pbus_register_protocols\n");
        device_remove(bus->zxdev);
    }
    return status;
}

static void platform_bus_release(void* ctx) {
    platform_bus_t* bus = ctx;

    platform_dev_t* dev;
    list_for_every_entry(&bus->devices, dev, platform_dev_t, node) {
        platform_dev_free(dev);
    }

    i2c_txn_t* txn;
    i2c_txn_t* temp;
    list_for_every_entry_safe(&bus->i2c_txns, txn, temp, i2c_txn_t, node) {
        free(txn);
    }

    free(bus);
}

static zx_protocol_device_t platform_bus_proto = {
    .version = DEVICE_OPS_VERSION,
    .release = platform_bus_release,
};

static zx_status_t sys_device_suspend(void* ctx, uint32_t flags) {
    switch (flags) {
    case DEVICE_SUSPEND_FLAG_REBOOT:
    case DEVICE_SUSPEND_FLAG_POWEROFF:
        // Kill this driver so that the IPC channel gets closed; devmgr will
        // perform a fallback that should shutdown or reboot the machine.
        exit(0);
    default:
        return ZX_ERR_NOT_SUPPORTED;
    };
}

static zx_protocol_device_t sys_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .suspend = sys_device_suspend,
};

static zx_status_t platform_bus_create(void* ctx, zx_device_t* parent, const char* name,
                                       const char* args, zx_handle_t rpc_channel) {
    if (!args) {
        zxlogf(ERROR, "platform_bus_create: args missing\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    uint32_t vid = 0;
    uint32_t pid = 0;
    if (sscanf(args, "vid=%u,pid=%u", &vid, &pid) != 2) {
        zxlogf(ERROR, "platform_bus_create: could not find vid or pid in args\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    platform_bus_t* bus = calloc(1, sizeof(platform_bus_t));
    if (!bus) {
        return  ZX_ERR_NO_MEMORY;
    }
printf("platform_bus_create %p\n", bus); 

    char* board_name = strstr(args, "board=");
    if (board_name) {
        board_name += strlen("board=");
        strncpy(bus->board_name, board_name, sizeof(bus->board_name));
        bus->board_name[sizeof(bus->board_name) - 1] = 0;
        char* comma = strchr(bus->board_name, ',');
        if (comma) {
            *comma = 0;
        }
    }

    // This creates the "sys" device
    device_add_args_t self_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = name,
        .ops = &sys_device_proto,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    zx_status_t status = device_add(parent, &self_args, &parent);
    if (status != ZX_OK) {
        return status;
    }

    // Then we attach the platform-bus device below it
    bus->resource = get_root_resource();
    bus->vid = vid;
    bus->pid = pid;
    list_initialize(&bus->devices);
    list_initialize(&bus->i2c_txns);
    mtx_init(&bus->i2c_txn_lock, mtx_plain);

    zx_device_prop_t props[] = {
        {BIND_PLATFORM_DEV_VID, 0, bus->vid},
        {BIND_PLATFORM_DEV_PID, 0, bus->pid},
    };

    char namestr[ZX_DEVICE_NAME_MAX];
    snprintf(namestr, sizeof(namestr), "%04x:%04x", vid, pid);

    device_add_args_t add_args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = namestr,
        .ctx = bus,
        .ops = &platform_bus_proto,
        .proto_id = ZX_PROTOCOL_PLATFORM_BUS,
        .proto_ops = &platform_bus_proto_ops,
        .props = props,
        .prop_count = countof(props),
    };

    return device_add(parent, &add_args, &bus->zxdev);
}

static zx_driver_ops_t platform_bus_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .create = platform_bus_create,
};

ZIRCON_DRIVER_BEGIN(platform_bus, platform_bus_driver_ops, "zircon", "0.1", 1)
    // devmgr loads us directly, so we need no binding information here
    BI_ABORT_IF_AUTOBIND,
ZIRCON_DRIVER_END(platform_bus)
