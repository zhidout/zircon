// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS;

typedef struct {
    zx_paddr_t  base;
    size_t      length;
} pbus_mmio_t;

typedef struct {
    uint32_t    irq;
} pbus_irq_t;

typedef struct {
    uint32_t    gpio;
} pbus_gpio_t;

typedef struct {
    uint32_t    bus_id;
    uint16_t    address;
} pbus_i2c_channel_t;

typedef struct {
    const char* name;
    uint32_t vid;
    uint32_t pid;
    uint32_t did;
    const pbus_mmio_t* mmios;
    uint32_t mmio_count;
    const pbus_irq_t* irqs;
    uint32_t irq_count;
    const pbus_gpio_t* gpios;
    uint32_t gpio_count;
    const pbus_i2c_channel_t* i2c_channels;
    uint32_t i2c_channel_count;
} pbus_dev_t;

typedef struct {
    uint32_t proto_id;
    void* ctx;
    void* ops;
} pbus_protocol_t;

// flags for pbus_device_add()
enum {
    // Add the device but to not publish it to the devmgr until enabled with pbus_device_enable().
    PDEV_ADD_DISABLED = (1 << 0),
};

typedef struct {
    zx_status_t (*register_protocols)(void* ctx, const pbus_protocol_t* protocols, size_t count);
    zx_status_t (*device_add)(void* ctx, const pbus_dev_t* dev, uint32_t flags);
    zx_status_t (*device_enable)(void* ctx, uint32_t vid, uint32_t pid, uint32_t did, bool enable);
    zx_status_t (*bus_device_add)(void* ctx, uint32_t vid, uint32_t pid, uint32_t did);
    const char* (*get_board_name)(void* ctx);
} platform_bus_protocol_ops_t;

typedef struct {
    platform_bus_protocol_ops_t* ops;
    void* ctx;
} platform_bus_protocol_t;

// Registers the protocols that a platform bus driver implements with the platform bus.
// The "count" is number of protocols being registered and ctxs[i] and ops[i] are the ith
// ctx and ops pointer for the ith protocol.
// Every driver that binds to ZX_PROTOCOL_PLATFORM_BUS must call this in their device bind hook.
static inline zx_status_t pbus_register_protocols(platform_bus_protocol_t* pbus,
                                                  const pbus_protocol_t* protocols, size_t count) {
    return pbus->ops->register_protocols(pbus->ctx, protocols, count);
}

static inline zx_status_t pbus_device_add(platform_bus_protocol_t* pbus, const pbus_dev_t* dev,
                                          uint32_t flags) {
    return pbus->ops->device_add(pbus->ctx, dev, flags);
}

// Dynamically enables or disables a platform device by adding or removing it
// from the DDK device tree.
static inline zx_status_t pbus_device_enable(platform_bus_protocol_t* pbus, uint32_t vid,
                                             uint32_t pid, uint32_t did, bool enable) {
    return pbus->ops->device_enable(pbus->ctx, vid, pid, did, enable);
}

static inline zx_status_t pbus_bus_device_add(platform_bus_protocol_t* pbus, uint32_t vid, uint32_t pid,
                                          uint32_t did) {
    return pbus->ops->bus_device_add(pbus->ctx, vid, pid, did);
}

static inline const char* pbus_get_board_name(platform_bus_protocol_t* pbus) {
    return pbus->ops->get_board_name(pbus->ctx);
}

__END_CDECLS;
