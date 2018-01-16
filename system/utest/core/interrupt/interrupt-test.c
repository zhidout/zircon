// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unittest/unittest.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/port.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

extern zx_handle_t get_root_resource(void);

// Tests support for virtual interrupts
static bool interrupt_test(void) {
    const uint32_t BOUND_SLOT = 0;
    const uint32_t UNBOUND_SLOT = 1;

    BEGIN_TEST;

    zx_handle_t handle;
    zx_handle_t rsrc = get_root_resource();
    uint64_t slots;
    zx_time_t timestamp;
    zx_time_t signaled_timestamp = 12345;

    ASSERT_EQ(zx_interrupt_create(rsrc, 0, &handle), ZX_OK, "");
    ASSERT_EQ(zx_interrupt_bind(handle, ZX_INTERRUPT_SLOT_USER, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL),
              ZX_ERR_ALREADY_BOUND, "");
    ASSERT_EQ(zx_interrupt_bind(handle, ZX_INTERRUPT_MAX_SLOTS + 1, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL),
              ZX_ERR_INVALID_ARGS, "");
    ASSERT_EQ(zx_interrupt_bind(handle, BOUND_SLOT, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL), ZX_OK, "");
    ASSERT_EQ(zx_interrupt_bind(handle, BOUND_SLOT, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL),
                                ZX_ERR_ALREADY_BOUND, "");

    ASSERT_EQ(zx_interrupt_get_timestamp(handle, BOUND_SLOT, &timestamp), ZX_ERR_BAD_STATE, "");

    ASSERT_EQ(zx_interrupt_signal(handle, UNBOUND_SLOT, signaled_timestamp),
                                  ZX_ERR_NOT_FOUND, "");
    ASSERT_EQ(zx_interrupt_signal(handle, BOUND_SLOT, signaled_timestamp), ZX_OK, "");

    ASSERT_EQ(zx_interrupt_wait(handle, &slots), ZX_OK, "");
    ASSERT_EQ(slots, (1ul << BOUND_SLOT), "");

    ASSERT_EQ(zx_interrupt_get_timestamp(handle, UNBOUND_SLOT, &timestamp), ZX_ERR_NOT_FOUND, "");
    ASSERT_EQ(zx_interrupt_get_timestamp(handle, BOUND_SLOT, &timestamp), ZX_OK, "");
    ASSERT_EQ(timestamp, signaled_timestamp, "");

    ASSERT_EQ(zx_handle_close(handle), ZX_OK, "");

    END_TEST;
}

// Tests support for multiple virtual interrupts
static bool interrupt_test_multiple(void) {
    BEGIN_TEST;

    zx_handle_t handle;
    zx_handle_t rsrc = get_root_resource();
    uint64_t slots;
    zx_time_t timestamp;
    zx_time_t signaled_timestamp = 1;

    ASSERT_EQ(zx_interrupt_create(rsrc, 0, &handle), ZX_OK, "");

    for (uint32_t slot = 0; slot < ZX_INTERRUPT_SLOT_USER; slot++) {
        ASSERT_EQ(zx_interrupt_bind(handle, slot, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL), ZX_OK, "");
    }

    for (uint32_t slot = 0; slot < ZX_INTERRUPT_SLOT_USER; slot++, signaled_timestamp++) {
        ASSERT_EQ(zx_interrupt_signal(handle, slot, signaled_timestamp), ZX_OK, "");
        ASSERT_EQ(zx_interrupt_wait(handle, &slots), ZX_OK, "");
        ASSERT_EQ(slots, (1ul << slot), "");
        ASSERT_EQ(zx_interrupt_get_timestamp(handle, slot, &timestamp), ZX_OK, "");
        ASSERT_EQ(timestamp, signaled_timestamp, "");
    }

    ASSERT_EQ(zx_handle_close(handle), ZX_OK, "");

    END_TEST;
}

// Tests support for interrupts with ports
static bool interrupt_test_port(void) {
    BEGIN_TEST;

    zx_handle_t interrupt;
    zx_handle_t port;
    zx_handle_t rsrc = get_root_resource();
    const uint32_t slot = 1;
    const uint64_t key = 0x12345678;
    zx_port_packet_t packet = {};
    uint64_t slots;

    ASSERT_EQ(zx_interrupt_create(rsrc, 0, &interrupt), ZX_OK, "");
    ASSERT_EQ(zx_interrupt_bind(interrupt, slot, ZX_HANDLE_INVALID, 0, ZX_INTERRUPT_VIRTUAL), ZX_OK, "");

    ASSERT_EQ(zx_port_create(0, &port), ZX_OK, "");

    ASSERT_EQ(zx_object_wait_async(interrupt, port, key, ZX_INTERRUPT_SIGNALED, ZX_WAIT_ASYNC_ONCE), ZX_OK, "");

    ASSERT_EQ(zx_port_wait(port, ZX_TIME_INFINITE, &packet, 0u), ZX_OK, "");
    ASSERT_EQ(packet.key, key, "");
    ASSERT_EQ(packet.type, ZX_PKT_TYPE_SIGNAL_ONE, "");
    ASSERT_EQ(packet.signal.observed, ZX_INTERRUPT_SIGNALED, "");
    ASSERT_EQ(packet.signal.trigger, ZX_INTERRUPT_SIGNALED, "");
    ASSERT_EQ(packet.signal.count, 1u, "");

    ASSERT_EQ(zx_interrupt_wait(interrupt, &slots), ZX_OK, "");
    ASSERT_EQ(slots, 1ul << slot, "");

    ASSERT_EQ(zx_handle_close(interrupt), ZX_OK, "");
    ASSERT_EQ(zx_handle_close(port), ZX_OK, "");

    END_TEST;
}

BEGIN_TEST_CASE(interrupt_tests)
RUN_TEST(interrupt_test)
RUN_TEST(interrupt_test_multiple)
RUN_TEST(interrupt_test_port)
END_TEST_CASE(interrupt_tests)
