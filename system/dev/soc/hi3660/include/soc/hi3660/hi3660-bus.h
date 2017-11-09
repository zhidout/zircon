// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <ddk/protocol/gpio.h>
#include <zircon/listnode.h>

typedef struct {
    list_node_t gpios;
    gpio_protocol_t gpio;
    io_buffer_t usb3otg_bc;
    io_buffer_t peri_crg;
    io_buffer_t pctrl;
} hi3660_bus_t;

zx_status_t hi3660_init(zx_handle_t resource, hi3660_bus_t** out_bus);
void hi3660_release(hi3660_bus_t* bus);

zx_status_t hi3660_gpio_init(hi3660_bus_t* bus);
void hi3660_gpio_release(hi3660_bus_t* bus);

zx_status_t hi3660_usb_init(hi3660_bus_t* bus);
