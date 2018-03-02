# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT


ifeq ($(PLATFORM_VID),)
$(error PLATFORM_VID not defined)
endif
ifeq ($(PLATFORM_PID),)
$(error PLATFORM_PID not defined)
endif
ifeq ($(PLATFORM_BOARD_NAME),)
$(error PLATFORM_BOARD_NAME not defined)
endif
ifeq ($(PLATFORM_MDI_SRCS),)
$(error PLATFORM_MDI_SRCS not defined)
endif

MDI_BIN := $(BUILDDIR)/$(PLATFORM_BOARD_NAME)-mdi.bin
PLATFORM_ID_BIN := $(BUILDDIR)/$(PLATFORM_BOARD_NAME)-platform-id.bin
    
# rule for building MDI binary blob
$(MDI_BIN): $(MDIGEN) $(PLATFORM_MDI_SRCS) $(MDI_INCLUDES)
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)$(MDIGEN) -o $@ $(PLATFORM_MDI_SRCS)

GENERATED += $(MDI_BIN)
EXTRA_BUILDDEPS += $(MDI_BIN)

# rule for building platform ID bootdata record
$(PLATFORM_ID_BIN): $(MKBOOTFS)
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)$(MKBOOTFS) -o $@ --vid $(PLATFORM_VID) --pid $(PLATFORM_PID) --board $(PLATFORM_BOARD_NAME)

GENERATED += $(PLATFORM_ID_BIN)
EXTRA_BUILDDEPS += $(PLATFORM_ID_BIN)
