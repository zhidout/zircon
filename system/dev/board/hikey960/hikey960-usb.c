// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/driver.h>
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-defs.h>
#include <hw/reg.h>
#include <soc/hi3660/hi3660-regs.h>
#include <soc/hi3660/hi3660-hw.h>

#include <stdio.h>

#include "hikey960.h"
#include "hikey960-hw.h"

zx_status_t hikey960_usb_set_mode(hikey960_t* bus, usb_mode_t mode) {
    if (mode == bus->usb_mode) {
        return ZX_OK;
    }

    gpio_protocol_t* gpio = &bus->hi3660->gpio;
    gpio_config(gpio, GPIO_HUB_VDD33_EN, GPIO_DIR_OUT);
    gpio_config(gpio, GPIO_VBUS_TYPEC, GPIO_DIR_OUT);
    gpio_config(gpio, GPIO_USBSW_SW_SEL, GPIO_DIR_OUT);

    gpio_write(gpio, GPIO_HUB_VDD33_EN, mode == USB_MODE_HOST);
    gpio_write(gpio, GPIO_VBUS_TYPEC, mode == USB_MODE_HOST);
    gpio_write(gpio, GPIO_USBSW_SW_SEL, mode == USB_MODE_HOST);

    // add or remove XHCI device
    pbus_device_enable(&bus->pbus, PDEV_VID_GENERIC, PDEV_PID_GENERIC, PDEV_DID_USB_XHCI,
                       mode == USB_MODE_HOST);

    bus->usb_mode = mode;
    return ZX_OK;
}
