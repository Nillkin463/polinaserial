#
# polinaserial makefile
#

MODULES := \
	app \
	drivers/serial

PROJ_NAME ?= polinaserial

CC ?= clang
PYTHON ?= python3

CODESIGN := codesign
CODESIGN_FLAGS := -s - -f

ARCHS := -arch x86_64 -arch arm64
MACOSX_MIN_VERSION := -mmacosx-version-min=10.7

BUILD_TAG_DB := polinaserial_tag_db.json
BUILD_TAG_FILE := .tag

$(shell $(PYTHON) polinatag.py generate . $(BUILD_TAG_DB) > $(BUILD_TAG_FILE))

CFLAGS := $(ARCHS)
CFLAGS += $(MACOSX_MIN_VERSION)
CFLAGS += -O3
CFLAGS += -Iinclude
CFLAGS += -MMD
CFLAGS += -DPRODUCT_NAME=\"$(PROJ_NAME)\"

LDFLAGS := $(ARCHS)
LDFLAGS += $(MACOSX_MIN_VERSION)
LDFLAGS += -framework CoreFoundation
LDFLAGS += -framework IOKit
LDFLAGS += -sectcreate __TEXT __build_tag $(BUILD_TAG_FILE)
LDFLAGS += -Wl,-no_adhoc_codesign

STYLE ?= RELEASE

ifeq ($(STYLE),ASAN)
	CFLAGS += -fsanitize=address
	LDFLAGS += -fsanitize=address
else ifeq ($(STYLE),PROFILING)
	CFLAGS += -gfull
	LDFLAGS += -gfull
	CODESIGN_FLAGS += --entitlements gta.entitlements
else ifeq ($(STYLE),RELEASE)
# nop
else ifneq ($(STYLE),)
$(error unknown STYLE)
endif

BUILD_ROOT := build
CURRENT_ROOT := $(BUILD_ROOT)/$(STYLE)

DIR_HELPER = mkdir -p $(@D)
GET_LOCAL_DIR = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

ifneq ($(STYLE),RELEASE)
	BINARY := $(BUILD_ROOT)/$(PROJ_NAME)-$(STYLE)
else
	BINARY := $(BUILD_ROOT)/$(PROJ_NAME)
endif

MODULES_INCLUDES := $(addsuffix /rules.mk,$(MODULES))

OPTIONS :=
OBJECTS :=

-include $(MODULES_INCLUDES)

.PHONY: clean all

all: $(BINARY)
	@$(shell $(PYTHON) polinatag.py commit $(BUILD_TAG_DB))
	@echo "%%%%% done building"

$(BINARY): $(OBJECTS)
	@echo "\tlinking"
	@$(DIR_HELPER)
	@$(CC) $(LDFLAGS) $^ -o $@
	@$(CODESIGN) $(CODESIGN_FLAGS) $@

$(CURRENT_ROOT)/%.o: %.c
	@echo "\tcompiling C: $<"
	@$(DIR_HELPER)
	@$(CC) $(CFLAGS) $(OPTIONS) -c $< -o $@ 

clean:
	$(shell rm -rf $(BUILD_ROOT))
	@echo "%%%%% done cleaning"

-include $(OBJECTS:.o=.d)
