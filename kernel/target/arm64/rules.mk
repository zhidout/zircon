# Copyright 2018 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

LOCAL_DIR := $(GET_LOCAL_DIR)

PLATFORM := generic-arm


MODULE := $(LOCAL_DIR)

LEGACY_LOADER_SRC := $(LOCAL_DIR)/legacy-loader.S
LEGACY_LOADER_OBJ := $(BUILDDIR)/legacy-loader.o
LEGACY_LOADER_BIN := $(BUILDDIR)/legacy-loader.bin
LEGACY_KERNEL_IMAGE := $(BUILDDIR)/legacy-zircon.bin

$(LEGACY_LOADER_OBJ): $(LEGACY_LOADER_SRC)
	@$(MKDIR)
	$(call BUILDECHO, compiling $<)
	$(NOECHO)$(CC) -Ikernel/arch/arm64/include -Isystem/public -c $< -MMD -MP -MT $@ -MF $(@:%o=%d) -o $@

$(LEGACY_LOADER_BIN): $(LEGACY_LOADER_OBJ)
	$(call BUILDECHO,generating $@)
	$(NOECHO)$(OBJCOPY) -O binary $< $@

GENERATED += $(LEGACY_LOADER_BIN)

# prepend legacy header to kernel image
$(LEGACY_KERNEL_IMAGE): $(LEGACY_LOADER_BIN) $(OUTLKBIN)
	$(NOECHO)cat $(LEGACY_LOADER_BIN) $(OUTLKBIN) > $(LEGACY_KERNEL_IMAGE)

EXTRA_KERNELDEPS += $(LEGACY_KERNEL_IMAGE)

# Some boards need gzipped kernel image
OUT_ZIRCON_ZIMAGE := $(BUILDDIR)/z$(LKNAME).bin

$(OUT_ZIRCON_ZIMAGE): $(LEGACY_KERNEL_IMAGE)
	$(call BUILDECHO,gzipping image $@)
	$(NOECHO)gzip -c $< > $@

GENERATED += $(OUT_ZIRCON_ZIMAGE)
EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE)

GENERATED += $(OUT_ZIRCON_ZIMAGE)
EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE)

# include rules for our various arm64 boards
include $(LOCAL_DIR)/*/rules.mk
