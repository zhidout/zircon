// Copyright 2018 The Fuchsia Authors.All rights reserved.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include <lib/async-testutils/test_loop_dispatcher.h>

#include <zircon/assert.h>
#include <zircon/syscalls.h>

#define TO_NODE(type, ptr) ((list_node_t*)&ptr->state)
#define FROM_NODE(type, ptr) ((type*)((char*)(ptr)-offsetof(type, state)))

namespace async {

namespace {

// Convenience functions for task, wait, and list node management.
inline list_node_t* WaitToNode(async_wait_t* wait) {
    return TO_NODE(async_wait_t, wait);
}

inline async_wait_t* NodeToWait(list_node_t* node) {
    return FROM_NODE(async_wait_t, node);
}

inline list_node_t* TaskToNode(async_task_t* task) {
    return TO_NODE(async_task_t, task);
}

inline async_task_t* NodeToTask(list_node_t* node) {
    return FROM_NODE(async_task_t, node);
}

inline void InsertTask(list_node_t* task_list, async_task_t* task) {
    list_node_t* node;
    for (node = task_list->prev; node != task_list; node = node->prev) {
        if (task->deadline >= NodeToTask(node)->deadline)
            break;
    }
    list_add_after(node, TaskToNode(task));
}

// async_t operations via those of the TestLoopDispatcher wrapper.
zx_time_t test_now(async_t* async) {
  return (static_cast<TestLoopDispatcher*>(async)->Now()).get();
}

zx_status_t test_begin_wait(async_t* async, async_wait_t* wait) {
    return static_cast<TestLoopDispatcher*>(async)->BeginWait(wait);
}

zx_status_t test_cancel_wait(async_t* async, async_wait_t* wait) {
    return static_cast<TestLoopDispatcher*>(async)->CancelWait(wait);
}

zx_status_t test_post_task(async_t* async, async_task_t* task) {
    return static_cast<TestLoopDispatcher*>(async)->PostTask(task);
}

zx_status_t test_cancel_task(async_t* async, async_task_t* task) {
    return static_cast<TestLoopDispatcher*>(async)->CancelTask(task);
}

zx_status_t test_queue_packet(async_t* async, async_receiver_t* receiver,
                              const zx_packet_user_t* data) {
    return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t test_set_guest_bell_trap(async_t* async, async_guest_bell_trap_t* trap) {
    return ZX_ERR_NOT_SUPPORTED;
}

const async_ops_t test_ops = {
    .now = test_now,
    .begin_wait = test_begin_wait,
    .cancel_wait = test_cancel_wait,
    .post_task = test_post_task,
    .cancel_task = test_cancel_task,
    .queue_packet = test_queue_packet,
    .set_guest_bell_trap = test_set_guest_bell_trap,
};

} // namespace

TestLoopDispatcher::TestLoopDispatcher(zx::time* current_time)
    : async_t{&test_ops}, current_time_(current_time) {
    ZX_DEBUG_ASSERT(current_time);
    list_initialize(&wait_list_);
    list_initialize(&task_list_);
    zx_status_t status = zx::port::create(0u, &port_);
    ZX_ASSERT_MSG(status == ZX_OK, "status=%d", status);
}

TestLoopDispatcher::~TestLoopDispatcher() = default;

zx_status_t TestLoopDispatcher::BeginWait(async_wait_t* wait) {
    ZX_DEBUG_ASSERT(wait);
    list_add_head(&wait_list_, WaitToNode(wait));

    zx_status_t status = zx_object_wait_async(wait->object, port_.get(),
                                              reinterpret_cast<uintptr_t>(wait),
                                              wait->trigger,
                                              ZX_WAIT_ASYNC_ONCE);

    if (status != ZX_OK) {
        // In this rare condition, the wait failed. Since a dispatched handler will
        // never be invoked on the wait object, we remove it ourselves.
        list_delete(WaitToNode(wait));
    }
    return status;
}

zx_status_t TestLoopDispatcher::CancelWait(async_wait_t* wait) {
    ZX_DEBUG_ASSERT(wait);
    zx_status_t status = port_.cancel(wait->object, reinterpret_cast<uintptr_t>(wait));
    if (status == ZX_OK) {
        list_delete(WaitToNode(wait));
    }
    return status;
}

zx_status_t TestLoopDispatcher::PostTask(async_task_t* task) {
    ZX_DEBUG_ASSERT(task);
    InsertTask(&task_list_, task);
    return ZX_OK;
}

zx_status_t TestLoopDispatcher::CancelTask(async_task_t* task) {
    ZX_DEBUG_ASSERT(task);
    list_node_t* node = TaskToNode(task);
    if (!list_in_list(node)) {
        return ZX_ERR_NOT_FOUND;
    }
    list_delete(node);
    return ZX_OK;
}

zx_status_t TestLoopDispatcher::DispatchNextWait() {
    zx_port_packet_t packet;
    zx_status_t status = port_.wait(zx::time(0), &packet, 0);
    if (status != ZX_OK) {
        return status;
    }
    async_wait_t* wait = reinterpret_cast<async_wait_t*>(packet.key);
    list_delete(WaitToNode(wait));
    async_wait_result_t result = wait->handler(this, wait, status, &packet.signal);

    if (result == ASYNC_WAIT_AGAIN) {
        status = zx_object_wait_async(wait->object, port_.get(),
                                      reinterpret_cast<uintptr_t>(wait),
                                      wait->trigger, ZX_WAIT_ASYNC_ONCE);
        if (status != ZX_OK) {
            wait->handler(this, wait, status, NULL);
            result = ASYNC_WAIT_FINISHED;
        }
    }

    if (result == ASYNC_WAIT_AGAIN) {
        list_add_head(&wait_list_, WaitToNode(wait));
    }
    return ZX_OK;
}

void TestLoopDispatcher::DispatchTasks() {
    zx::time dispatch_time = *current_time_;

    // Dequeue and dispatch one task at a time in case an earlier task wants
    // to cancel a later task which has also come due.
    list_node_t* node;
    while ((node = list_peek_head(&task_list_))) {
        async_task_t* task = NodeToTask(node);
        if (task->deadline > current_time_->get()) {
            break;
        }
        list_delete(node);

        // Invoke the handler. Note that it might destroy itself.
        async_task_result_t result = task->handler(this, task, ZX_OK);
        if (result == ASYNC_TASK_REPEAT) {
            InsertTask(&task_list_, task);
        }
    }

    // The following check fails when a task captures the TestLoop and advances
    // time. For one thing, this prevents the prospect of an infinte run.
    ZX_ASSERT(dispatch_time == *current_time_);
}

void TestLoopDispatcher::Shutdown() {
    list_node_t* node;
    while ((node = list_remove_head(&wait_list_))) {
        async_wait_t* wait = NodeToWait(node);
        if (wait->flags & ASYNC_FLAG_HANDLE_SHUTDOWN) {
            wait->handler(this, wait, ZX_ERR_CANCELED, NULL);
        }
    }
    while ((node = list_remove_head(&task_list_))) {
        async_task_t* task = NodeToTask(node);
        if (task->flags & ASYNC_FLAG_HANDLE_SHUTDOWN) {
            task->handler(this, task, ZX_ERR_CANCELED);
        }
    }
}

} // namespace async
