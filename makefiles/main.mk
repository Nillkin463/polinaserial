MODULES := \
	app \
	drivers/serial \
	drivers/random

GET_LOCAL_DIR = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

OPTIONS :=
OBJECTS :=

include $(GET_LOCAL_DIR)/platforms/$(PLATFORM).mk

PYTHON ?= python3

CODESIGN := codesign
CODESIGN_FLAGS := -s - -f

CFLAGS += $(ARCHS)
CFLAGS += $(OS_MIN_VERSION)
CFLAGS += -O3
CFLAGS += -Iapp/include
CFLAGS += -MMD
CFLAGS += -DPRODUCT_NAME=\"$(PROJ_NAME)\"

LDFLAGS += $(ARCHS)
LDFLAGS += $(OS_MIN_VERSION)
LDFLAGS += -framework CoreFoundation
LDFLAGS += -framework IOKit
LDFLAGS += -sectcreate __TEXT __build_tag $(BUILD_TAG_FILE)
LDFLAGS += -Wl,-no_adhoc_codesign

ifeq ($(STYLE),ASAN)
	CFLAGS += -fsanitize=address
	LDFLAGS += -fsanitize=address
	CFLAGS += -gfull
	LDFLAGS += -gfull
else ifeq ($(STYLE),PROFILING)
	CFLAGS += -gfull
	LDFLAGS += -gfull
	CODESIGN_FLAGS += --entitlements gta.entitlements
else ifneq ($(STYLE),RELEASE)
$(error unknown STYLE)
endif

CURRENT_ROOT := $(BUILD_ROOT)/$(PLATFORM)/$(STYLE)

DIR_HELPER = mkdir -p $(@D)

ifneq ($(STYLE),RELEASE)
	BINARY := $(BUILD_ROOT)/$(PLATFORM)/$(PROJ_NAME)-$(STYLE)
else
	BINARY := $(BUILD_ROOT)/$(PLATFORM)/$(PROJ_NAME)
endif

MODULES_INCLUDES := $(addsuffix /rules.mk,$(MODULES))

-include $(MODULES_INCLUDES)

.PHONY: clean all

all: $(BINARY)
	@echo "%%%%% done building"

$(BINARY): $(OBJECTS)
	@echo "\tlinking"
	@$(DIR_HELPER)
	@$(CC) $(LDFLAGS) $^ -o $@
	@echo "\tcodesigning"
	@$(CODESIGN) $(CODESIGN_FLAGS) $@

$(CURRENT_ROOT)/%.o: %.c
	@echo "\tcompiling C: $<"
	@$(DIR_HELPER)
	@$(CC) $(CFLAGS) $(OPTIONS) -c $< -o $@ 

-include $(OBJECTS:.o=.d)
