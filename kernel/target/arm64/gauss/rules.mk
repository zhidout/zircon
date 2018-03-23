# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

LOCAL_DIR := $(GET_LOCAL_DIR)

DEVICE_TREE := $(GET_LOCAL_DIR)/device-tree.dtb

PLATFORM_VID := 3   # PDEV_VID_GOOGLE
PLATFORM_PID := 1   # PDEV_PID_GAUSS
PLATFORM_BOARD_NAME := gauss
PLATFORM_MDI_SRCS := $(LOCAL_DIR)/gauss.mdi
PLATFORM_CMDLINE := $(LOCAL_DIR)/cmdline.txt

USE_LEGACY_LOADER := true

include make/board.mk

KDTBTOOL=$(BUILDDIR)/tools/mkkdtb

OUT_ZIRCON_ZIMAGE_KDTB := $(BUILDDIR)/z$(LKNAME).kdtb

BOARD_ZIRCON := $(BUILDDIR)/gauss-zircon.bin
BOARD_ZZIRCON := $(BUILDDIR)/gauss-zzircon.bin

$(BOARD_ZZIRCON): $(BOARD_ZIRCON)
	$(call BUILDECHO,gzipping image $@)
	$(NOECHO)gzip -c $< > $@

$(OUT_ZIRCON_ZIMAGE_KDTB): $(BOARD_ZZIRCON) $(DEVICE_TREE) $(KDTBTOOL)
	$(call BUILDECHO,generating $@)
	$(NOECHO)$(KDTBTOOL) $(BOARD_ZZIRCON) $(DEVICE_TREE) $@

EXTRA_BUILDDEPS += $(OUT_ZIRCON_ZIMAGE_KDTB)
GENERATED += $(OUT_ZIRCON_ZIMAGE_KDTB)
