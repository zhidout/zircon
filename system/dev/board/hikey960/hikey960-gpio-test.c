// Copyright 2017 The Fuchsia Authors. All rights reserved.
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
#include <ddk/protocol/platform-device.h>

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/assert.h>

// GPIO indices
enum {
    GPIO_LED1,
    GPIO_LED2,
    GPIO_LED3,
    GPIO_LED4,
    GPIO_POWER_BUTTON,
};

// GPIO interrupt slots
enum {
    GPIO_SLOT_POWER_BUTTON,
};

typedef struct {
    zx_device_t* zxdev;
    gpio_protocol_t gpio;
    thrd_t led_thread;
    thrd_t button_thread;
    zx_handle_t interrupt_handle;
    bool done;
} gpio_test_t;

static void gpio_test_release(void* ctx) {
    gpio_test_t* gpio_test = ctx;

    gpio_test->done = true;
    zx_interrupt_signal(gpio_test->interrupt_handle, ZX_INTERRUPT_SLOT_USER, 0);
    thrd_join(gpio_test->led_thread, NULL);
    thrd_join(gpio_test->button_thread, NULL);
    zx_handle_close(gpio_test->interrupt_handle);
    free(gpio_test);
}

static zx_protocol_device_t gpio_test_device_protocol = {
    .version = DEVICE_OPS_VERSION,
    .release = gpio_test_release,
};

// test thread that cycles the 4 LEDs on the hikey 960 board
static int led_test_thread(void *arg) {
    gpio_test_t* gpio_test = arg;
    gpio_protocol_t* gpio = &gpio_test->gpio;

    uint32_t led_gpios[] = { GPIO_LED1, GPIO_LED2, GPIO_LED3, GPIO_LED4 };

    for (unsigned i = 0; i < countof(led_gpios); i++) {
        gpio_config(gpio, led_gpios[i], GPIO_DIR_OUT);
    }

    while (!gpio_test->done) {
         for (unsigned i = 0; i < countof(led_gpios); i++) {
            gpio_write(gpio, led_gpios[i], 1);
            sleep(1);
            gpio_write(gpio, led_gpios[i], 0);
        }
    }

    return 0;
}


// test thread that monitors hikey 960 power button
static int button_test_thread(void *arg) {
    gpio_test_t* gpio_test = arg;
    zx_handle_t interrupt_handle = gpio_test->interrupt_handle;

    while (!gpio_test->done) {
        uint64_t slots;
        zx_status_t status = zx_interrupt_wait(interrupt_handle, &slots);
        if (status != ZX_OK) {
            zxlogf(ERROR, "button_test_thread: zx_interrupt_wait failed %d\n", status);
            break;
        }
        if (slots & ZX_INTERRUPT_SLOT_USER) {
            break;
        }

        if (slots & GPIO_SLOT_POWER_BUTTON) {
            uint8_t value;
            status = gpio_read(&gpio_test->gpio, GPIO_POWER_BUTTON, &value);

            if (value) {
                zxlogf(INFO, "button_test_thread: DOWN\n");
            } else {
                zxlogf(INFO, "button_test_thread: UP\n");
            }
        }
    }

    return 0;
}

static zx_status_t gpio_test_bind(void* ctx, zx_device_t* parent) {
    gpio_test_t* gpio_test = calloc(1, sizeof(gpio_test_t));
    if (!gpio_test) {
        return ZX_ERR_NO_MEMORY;
    }

    platform_device_protocol_t pdev;

    if (device_get_protocol(parent, ZX_PROTOCOL_PLATFORM_DEV, &pdev) != ZX_OK) {
        free(gpio_test);
        return ZX_ERR_NOT_SUPPORTED;
    }
    if (device_get_protocol(parent, ZX_PROTOCOL_GPIO, &gpio_test->gpio) != ZX_OK) {
        free(gpio_test);
        return ZX_ERR_NOT_SUPPORTED;
    }

    zx_status_t status = pdev_create_interrupt_handle(&pdev, &gpio_test->interrupt_handle);
    if (status != ZX_OK) {
        zxlogf(ERROR, "gpio_test_bind: gpio_get_event_handle failed %d\n", status);
        goto fail;
    }

    gpio_config_flags_t flags = GPIO_DIR_IN | GPIO_TRIGGER_EDGE |
                                GPIO_TRIGGER_RISING | GPIO_TRIGGER_FALLING;
    status = gpio_config(&gpio_test->gpio, GPIO_POWER_BUTTON, flags);
    if (status != ZX_OK) {
        zxlogf(ERROR, "gpio_test_bind: gpio_config failed %d\n", status);
        goto fail;
    }

    status = gpio_bind_interrupt(&gpio_test->gpio, GPIO_POWER_BUTTON,
                                 gpio_test->interrupt_handle, GPIO_SLOT_POWER_BUTTON);
    if (status != ZX_OK) {
        zxlogf(ERROR, "gpio_test_bind: gpio_bind_interrupt failed %d\n", status);
        goto fail;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "hi3660-gpio-test",
        .ctx = gpio_test,
        .ops = &gpio_test_device_protocol,
        .flags = DEVICE_ADD_NON_BINDABLE,
    };

    status = device_add(parent, &args, NULL);
    if (status != ZX_OK) {
        zx_handle_close(gpio_test->interrupt_handle);
        free(gpio_test);
        return status;
    }

    thrd_create_with_name(&gpio_test->led_thread, led_test_thread, gpio_test, "led_test_thread");
    thrd_create_with_name(&gpio_test->button_thread, button_test_thread, gpio_test,
                          "button_test_thread");
    return ZX_OK;

fail:
    zx_handle_close(gpio_test->interrupt_handle);
    free(gpio_test);
    return status;
}

static zx_driver_ops_t gpio_test_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = gpio_test_bind,
};

ZIRCON_DRIVER_BEGIN(hi3660_gpio_test, gpio_test_driver_ops, "zircon", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PLATFORM_DEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_96BOARDS),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_HIKEY960),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_HIKEY960_GPIO_TEST),
ZIRCON_DRIVER_END(hi3660_gpio_test)
