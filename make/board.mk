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

BOARD_MDI := $(BUILDDIR)/$(PLATFORM_BOARD_NAME)-mdi.bin
BOARD_PLATFORM_ID := --vid $(PLATFORM_VID) --pid $(PLATFORM_PID) --board $(PLATFORM_BOARD_NAME)
BOARD_BOOTDATA := $(BUILDDIR)/$(PLATFORM_BOARD_NAME)-bootdata.bin

$(BOARD_MDI): PLATFORM_MDI_SRCS:=$(PLATFORM_MDI_SRCS)
$(BOARD_BOOTDATA): BOARD_MDI:=$(BOARD_MDI)
$(BOARD_BOOTDATA): BOARD_BOOTDATA:=$(BOARD_BOOTDATA)

# rule for building MDI binary blob
$(BOARD_MDI): $(MDIGEN) $(PLATFORM_MDI_SRCS) $(MDI_INCLUDES)
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)$(MDIGEN) -o $@ $(PLATFORM_MDI_SRCS)

GENERATED += $(BOARD_MDI)
EXTRA_BUILDDEPS += $(BOARD_MDI)

$(BOARD_BOOTDATA): $(MKBOOTFS) $(USER_BOOTDATA) $(BOARD_MDI)
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)$(MKBOOTFS) -o $@ $(BOARD_PLATFORM_ID) $(USER_BOOTDATA) $(BOARD_MDI)

GENERATED += $(BOARD_BOOTDATA)
EXTRA_BUILDDEPS += $(BOARD_BOOTDATA)

# clear variables passed in
PLATFORM_MDI_SRCS :=
BOARD_PLATFORM_ID :=

# clear variables we set here
BOARD_MDI :=
BOARD_BOOTDATA :=
BOARD_PLATFORM_ID :=
