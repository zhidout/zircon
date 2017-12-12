// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <zircon/types.h>
#include <fbl/array.h>
#include <fbl/canary.h>
#include <fbl/mutex.h>
#include <object/interrupt_dispatcher.h>
#include <sys/types.h>

class InterruptEventDispatcher final : public InterruptDispatcher {

    struct Vector {
        zx_time_t timestamp;
        uint32_t flags;
        uint32_t index;
    };

public:
    static zx_status_t Create(fbl::RefPtr<Dispatcher>* dispatcher,
                              zx_rights_t* rights);

    InterruptEventDispatcher(const InterruptDispatcher &) = delete;
    InterruptEventDispatcher& operator=(const InterruptDispatcher &) = delete;

    ~InterruptEventDispatcher() final;

    zx_status_t Bind(uint32_t slot, uint32_t vector, uint32_t options) final;
    zx_status_t Unbind(uint32_t slot) final;
    zx_status_t WaitForInterrupt(zx_time_t deadline, uint64_t& out_slots) final;
    zx_status_t WaitForInterruptWithTimeStamp(zx_time_t deadline, uint32_t& out_slot,
                                              zx_time_t& out_timestamp) final;
    zx_status_t UserSignal() final;

private:
    explicit InterruptEventDispatcher() : bound_slots_(0) {}

    static enum handler_return IrqHandler(void* ctx);

    fbl::Canary<fbl::magic("INED")> canary_;

    // vectors bound to this dispatcher
    fbl::Array<Vector> vectors_ TA_GUARDED(vectors_lock_);
    uint64_t bound_slots_ TA_GUARDED(vectors_lock_);
    fbl::Mutex vectors_lock_;
};
