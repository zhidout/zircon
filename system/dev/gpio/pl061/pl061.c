// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <hw/reg.h>
#include <limits.h>
#include <stdlib.h>

#include <gpio/pl061/pl061.h>

// GPIO register offsets
#define GPIODATA(mask)  ((mask) << 2)   // Data registers, mask provided as index
#define GPIODIR         0x400           // Data direction register (0 = IN, 1 = OUT)
#define GPIOIS          0x404           // Interrupt sense register (0 = edge, 1 = level)
#define GPIOIBE         0x408           // Interrupt both edges register (1 = both)
#define GPIOIEV         0x40C           // Interrupt event register (0 = falling, 1 = rising)
#define GPIOIE          0x410           // Interrupt mask register (1 = interrupt masked)
#define GPIORIS         0x414           // Raw interrupt status register
#define GPIOMIS         0x418           // Masked interrupt status register
#define GPIOIC          0x41C           // Interrupt clear register
#define GPIOAFSEL       0x420           // Mode control select register

#define GPIOS_PER_PAGE  8

zx_status_t pl061_init(pl061_gpios_t* gpios, uint32_t gpio_start, uint32_t gpio_count,
                       const uint32_t* irqs, uint32_t irq_count,
                       zx_paddr_t mmio_base, size_t mmio_length, zx_handle_t resource) {
    zx_status_t status = io_buffer_init_physical(&gpios->buffer, mmio_base, mmio_length, resource,
                                                ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (status != ZX_OK) {
        zxlogf(ERROR, "pl061_init: io_buffer_init_physical failed %d\n", status);
        return status;
    }

    gpios->client_irq_handles = calloc(gpio_count, sizeof(*gpios->client_irq_handles));
    if (!gpios->client_irq_handles) {
        return ZX_ERR_NO_MEMORY;
    }
    gpios->client_irq_slots = calloc(gpio_count, sizeof(*gpios->client_irq_slots));
    if (!gpios->client_irq_slots) {
        free(gpios->client_irq_handles);
        return ZX_ERR_NO_MEMORY;
    }

    mtx_init(&gpios->lock, mtx_plain);
    gpios->gpio_start = gpio_start;
    gpios->gpio_count = gpio_count;
    gpios->irqs = irqs;
    gpios->irq_count = irq_count;
    gpios->resource = resource;

    return ZX_OK;
}

void pl061_release(pl061_gpios_t* gpios) {
    // stop interrupt thread
    if (gpios->irq_handle != ZX_HANDLE_INVALID) {
        zx_interrupt_signal(gpios->irq_handle, ZX_INTERRUPT_SLOT_USER, 0);
        thrd_join(gpios->irq_thread, NULL);
        zx_handle_close(gpios->irq_handle);
    }

    // close all client interrupt handles
    for (uint32_t i = 0; i < gpios->irq_count; i++) {
        zx_handle_close(gpios->client_irq_handles[i]);
    }

    io_buffer_release(&gpios->buffer);
    free(gpios->client_irq_handles);
    free(gpios->client_irq_slots);
    free(gpios);
}

static zx_status_t pl061_gpio_config(void* ctx, uint32_t index, gpio_config_flags_t flags) {
    pl061_gpios_t* gpios = ctx;
    if (index < gpios->gpio_start || index - gpios->gpio_start >= gpios->gpio_count) {
        return ZX_ERR_INVALID_ARGS;
    }
    index -= gpios->gpio_start;
    volatile uint8_t* regs = io_buffer_virt(&gpios->buffer) + PAGE_SIZE * (index / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (index % GPIOS_PER_PAGE);

    mtx_lock(&gpios->lock);
    uint8_t dir = readb(regs + GPIODIR);
    if ((flags & GPIO_DIR_MASK) == GPIO_DIR_OUT) {
        dir |= bit;
    } else {
        dir &= ~bit;
    }
    writeb(dir, regs + GPIODIR);

    uint8_t trigger = readb(regs + GPIOIS);
    if ((flags & GPIO_TRIGGER_MASK) == GPIO_TRIGGER_LEVEL) {
        trigger |= bit;
    } else {
        trigger &= ~bit;
    }
    writeb(trigger, regs + GPIOIS);

    uint8_t be = readb(regs + GPIOIBE);
    uint8_t iev = readb(regs + GPIOIEV);

    if ((flags & GPIO_TRIGGER_MASK) == GPIO_TRIGGER_EDGE && (flags & GPIO_TRIGGER_RISING)
        && (flags & GPIO_TRIGGER_FALLING)) {
        be |= bit;
     } else {
        be &= ~bit;
     }
    if ((flags & GPIO_TRIGGER_MASK) == GPIO_TRIGGER_EDGE && (flags & GPIO_TRIGGER_RISING)
        && !(flags & GPIO_TRIGGER_FALLING)) {
        iev |= bit;
     } else {
        iev &= ~bit;
     }

    writeb(be, regs + GPIOIBE);
    writeb(iev, regs + GPIOIEV);

    mtx_unlock(&gpios->lock);
    return ZX_OK;
}

static zx_status_t pl061_gpio_read(void* ctx, uint32_t index, uint8_t* out_value) {
    pl061_gpios_t* gpios = ctx;
    if (index < gpios->gpio_start || index - gpios->gpio_start >= gpios->gpio_count) {
        return ZX_ERR_INVALID_ARGS;
    }
    index -= gpios->gpio_start;
    volatile uint8_t* regs = io_buffer_virt(&gpios->buffer) + PAGE_SIZE * (index / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (index % GPIOS_PER_PAGE);

    *out_value = !!(readb(regs + GPIODATA(bit)) & bit);
    return ZX_OK;
}

static zx_status_t pl061_gpio_write(void* ctx, uint32_t index, uint8_t value) {
    pl061_gpios_t* gpios = ctx;
    if (index < gpios->gpio_start || index - gpios->gpio_start >= gpios->gpio_count) {
        return ZX_ERR_INVALID_ARGS;
    }
    index -= gpios->gpio_start;
    volatile uint8_t* regs = io_buffer_virt(&gpios->buffer) + PAGE_SIZE * (index / GPIOS_PER_PAGE);
    uint8_t bit = 1 << (index % GPIOS_PER_PAGE);

    writeb((value ? bit : 0), regs + GPIODATA(bit));
    return ZX_OK;
}

zx_status_t pl061_gpio_bind_interrupt(void* ctx, uint32_t index, zx_handle_t handle,
                                      uint32_t slot) {
    pl061_gpios_t* gpios = ctx;
    if (index < gpios->gpio_start || index - gpios->gpio_start >= gpios->gpio_count) {
        return ZX_ERR_INVALID_ARGS;
    }
    index -= gpios->gpio_start;
    if (gpios->client_irq_handles[index] != ZX_HANDLE_INVALID) {
        return ZX_ERR_ALREADY_BOUND;
    }

// BLAA BLAA

    gpios->client_irq_handles[index] = handle;
    gpios->client_irq_slots[index] = slot;

    return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t pl061_gpio_unbind_interrupt(void* ctx, uint32_t index, zx_handle_t handle) {
    pl061_gpios_t* gpios = ctx;
    if (index < gpios->gpio_start || index - gpios->gpio_start >= gpios->gpio_count) {
        return ZX_ERR_INVALID_ARGS;
    }
    index -= gpios->gpio_start;
    if (gpios->client_irq_handles[index] != handle) {
        return ZX_ERR_INVALID_ARGS;
    }

    zx_handle_close(gpios->client_irq_handles[index]);
    gpios->client_irq_handles[index] = ZX_HANDLE_INVALID;

    return ZX_OK;
}

gpio_protocol_ops_t pl061_proto_ops = {
    .config = pl061_gpio_config,
    .read = pl061_gpio_read,
    .write = pl061_gpio_write,
    .bind_interrupt = pl061_gpio_bind_interrupt,
    .unbind_interrupt = pl061_gpio_unbind_interrupt,
};
