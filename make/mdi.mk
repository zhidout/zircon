# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

GEN_HEADER_DIR := $(BUILDDIR)/gen/global/include
MDI_HEADER_DIR := $(GEN_HEADER_DIR)/mdi
MDI_PATH := $(BUILDDIR)/$(MDI_BIN)

# rule for building MDI binary blob
$(MDI_PATH): $(MDIGEN) $(MDI_SRCS) $(MDI_INCLUDES)
	$(call BUILDECHO,generating $@)
	@$(MKDIR)
	$(NOECHO)$(MDIGEN) -o $@ $(MDI_SRCS)

GENERATED += $(MDI_PATH)
EXTRA_BUILDDEPS += $(MDI_PATH)
