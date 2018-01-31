// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <soc/aml-s912/s912-gpio.h>

static aml_gpio_block_t s912_blocks[] = {

    // GPIOX Block
    {
        .pin_count = S912_GPIOX_PINS,
        .oen_offset = S912_GPIOX_0EN,
        .input_offset = S912_GPIOX_IN,
        .output_offset = S912_GPIOX_OUT,
        .input_shift = 0,
        .output_shift = 0,
        .mmio_index = 0,
        .lock = MTX_INIT,
    },
    // GPIODV Block
    {
        .pin_count = S912_GPIOX_PINS,
        .oen_offset = S912_GPIODV_0EN,
        .input_offset = S912_GPIODV_IN,
        .output_offset = S912_GPIODV_OUT,
        .input_shift = 0,
        .output_shift = 0,
        .mmio_index = 0,
        .lock = MTX_INIT,
    },
};
