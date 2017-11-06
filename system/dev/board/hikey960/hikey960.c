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
#include <ddk/protocol/gpio.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/platform-bus.h>
#include <ddk/protocol/usb-mode-switch.h>

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

#include "hikey960-hw.h"

#define GPIO_TEST 1

typedef struct {
    zx_device_t* zxdev;
    zx_device_t* parent;
    platform_bus_protocol_t pbus;
    usb_mode_t usb_mode;
} hikey960_bus_t;


#if GPIO_TEST
static const pbus_gpio_t gpio_test_gpios[] = {
    {
        .gpio = GPIO_USER_LED1,
    },
    {
        .gpio = GPIO_USER_LED2,
    },
    {
        .gpio = GPIO_USER_LED3,
    },
    {
        .gpio = GPIO_USER_LED4,
    },
};

static const pbus_dev_t gpio_test_dev = {
    .name = "gpio-test",
    .vid = PDEV_VID_96BOARDS,
    .pid = PDEV_PID_HIKEY960,
    .did = PDEV_DID_HIKEY960_GPIO_TEST,
    .gpios = gpio_test_gpios,
    .gpio_count = countof(gpio_test_gpios),
};
#endif

static zx_status_t hikey_get_initial_mode(void* ctx, usb_mode_t* out_mode) {
    *out_mode = USB_MODE_HOST;
    return ZX_OK;
}

zx_status_t hikey_usb_set_mode(void* ctx, usb_mode_t mode) {
    hikey960_bus_t* bus = ctx;

    if (mode == USB_MODE_OTG) {
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (mode == bus->usb_mode) {
        return ZX_OK;
    }

    gpio_protocol_t gpio;
    zx_status_t status = device_get_protocol(bus->parent, ZX_PROTOCOL_GPIO, &gpio);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hikey_usb_set_mode could not get GPIO protocol\n");
        return status;
    }

    gpio_config(&gpio, GPIO_HUB_VDD33_EN, GPIO_DIR_OUT);
    gpio_config(&gpio, GPIO_VBUS_TYPEC, GPIO_DIR_OUT);
    gpio_config(&gpio, GPIO_USBSW_SW_SEL, GPIO_DIR_OUT);

    gpio_write(&gpio, GPIO_HUB_VDD33_EN, mode == USB_MODE_HOST);
    gpio_write(&gpio, GPIO_VBUS_TYPEC, mode == USB_MODE_HOST);
    gpio_write(&gpio, GPIO_USBSW_SW_SEL, mode == USB_MODE_HOST);

    // add or remove XHCI device
    pbus_device_enable(&bus->pbus, PDEV_VID_GENERIC, PDEV_PID_GENERIC, PDEV_DID_USB_XHCI,
                       mode == USB_MODE_HOST);

    bus->usb_mode = mode;
    return ZX_OK;
}

usb_mode_switch_protocol_ops_t usb_mode_switch_ops = {
    .get_initial_mode = hikey_get_initial_mode,
    .set_mode = hikey_usb_set_mode,
};

static void hikey_release(void* ctx) {
    hikey960_bus_t* bus = ctx;
    free(bus);
}

static zx_protocol_device_t hikey_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = hikey_release,
};

static zx_status_t hikey_bind(void* ctx, zx_device_t* parent, void** cookie) {
    hikey960_bus_t* bus = calloc(1, sizeof(hikey960_bus_t));
    if (!bus) {
        return ZX_ERR_NO_MEMORY;
    }
    bus->parent = parent;
    bus->usb_mode = USB_MODE_NONE;

    if (device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_BUS, &bus->pbus) != ZX_OK) {
        free(bus);
        return ZX_ERR_NOT_SUPPORTED;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "hikey960-bus",
        .ctx = bus,
        .ops = &hikey_device_protocol,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    zx_status_t status = device_add(parent, &args, &bus->zxdev);
    if (status != ZX_OK) {
        goto fail;
    }

    const pbus_protocol_t protocols[] = {
        {
            .proto_id = ZX_PROTOCOL_USB_MODE_SWITCH,
            .ctx = bus,
            .ops = &usb_mode_switch_ops,
        },
    };

    if ((status = pbus_register_protocols(&bus->pbus,  protocols, countof(protocols))) != ZX_OK) {
        zxlogf(ERROR, "hikey_bind: pbus_register_protocols failed!\n");;
    }

    // load SOC driver
    status = pbus_bus_device_add(&bus->pbus, PDEV_VID_HI_SILICON, PDEV_PID_HI3660, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hikey_bind could not add hi3660 driver: %d\n", status);
    }

#if GPIO_TEST
    // add GPIO test driver
    status = pbus_device_add(&bus->pbus, &gpio_test_dev, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "hikey_bind could not add gpio_test_dev: %d\n", status);
    }
#endif

    return ZX_OK;

fail:
    zxlogf(ERROR, "hikey_bind failed %d\n", status);
    hikey_release(bus);
    return status;
}

static zx_driver_ops_t hikey_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = hikey_bind,
};

ZIRCON_DRIVER_BEGIN(hikey, hikey_driver_ops, "zircon", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_BUS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_96BOARDS),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_HIKEY960),
ZIRCON_DRIVER_END(hikey)
