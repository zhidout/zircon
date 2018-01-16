// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/compiler.h>
#include <zircon/types.h>

__BEGIN_CDECLS;

// flags for gpio_config()
typedef enum {
    GPIO_DIR_IN             = 0 << 0,
    GPIO_DIR_OUT            = 1 << 0,
    GPIO_DIR_MASK           = 1 << 0,

    GPIO_TRIGGER_EDGE       = 0 << 1,
    GPIO_TRIGGER_LEVEL      = 1 << 1,
    GPIO_TRIGGER_MASK       = 1 << 1,

    // for edge triggered
    GPIO_TRIGGER_RISING     = 1 << 2,
    GPIO_TRIGGER_FALLING    = 1 << 3,

    // for level triggered
    GPIO_TRIGGER_HIGH       = 1 << 2,
    GPIO_TRIGGER_LOW        = 1 << 3,
} gpio_config_flags_t;

// In the functions below, the GPIO index is relative to the list of GPIOs for the device.
// For example, the list of GPIOs a platform device has access to would likely be a small
// subset of the total number of GPIOs, while a platform bus implementation driver would
// have access to the complete set of GPIOs.

typedef struct {
    zx_status_t (*config)(void* ctx, uint32_t index, gpio_config_flags_t flags);
    zx_status_t (*read)(void* ctx, uint32_t index, uint8_t* out_value);
    zx_status_t (*write)(void* ctx, uint32_t index, uint8_t value);
    zx_status_t (*bind_interrupt)(void* ctx, uint32_t index, zx_handle_t handle, uint32_t slot);
    zx_status_t (*unbind_interrupt)(void* ctx, uint32_t index, zx_handle_t handle);
} gpio_protocol_ops_t;

typedef struct {
    gpio_protocol_ops_t* ops;
    void* ctx;
} gpio_protocol_t;

// configures a GPIO
static inline zx_status_t gpio_config(gpio_protocol_t* gpio, uint32_t index,
                                      gpio_config_flags_t flags) {
    return gpio->ops->config(gpio->ctx, index, flags);
}

// reads the current value of a GPIO (0 or 1)
static inline zx_status_t gpio_read(gpio_protocol_t* gpio, uint32_t index, uint8_t* out_value) {
    return gpio->ops->read(gpio->ctx, index, out_value);
}

// sets the current value of the GPIO (any non-zero value maps to 1)
static inline zx_status_t gpio_write(gpio_protocol_t* gpio, uint32_t index, uint8_t value) {
    return gpio->ops->write(gpio->ctx, index, value);
}

// Binds the interrupt for the GPIO specified by "index" to the given slot on an interupt handle.
// Returns ZX_ERR_ALREADY_BOUND if another interrupt handle has been bound to the GPIO.
static inline zx_status_t gpio_bind_interrupt(gpio_protocol_t* gpio, uint32_t index,
                                              zx_handle_t handle, uint32_t slot) {
    return gpio->ops->bind_interrupt(gpio->ctx, index, handle, slot);
}

// Binds the interrupt handle from the GPIO specified by "index".
// Returns ZX_ERR_INVALID_ARGS if the interrupt handle is not currently bound to the GPIO.
static inline zx_status_t gpio_unbind_interrupt(gpio_protocol_t* gpio, uint32_t index,
                                              zx_handle_t handle) {
    return gpio->ops->unbind_interrupt(gpio->ctx, index, handle);
}

__END_CDECLS;
