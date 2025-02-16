#
# polinaserial makefile
#

MODULES := \
	app \
	drivers/serial

PROJ_NAME ?= polinaserial

PYTHON ?= python3

CC ?= clang

ARCHS := -arch x86_64 -arch arm64
MACOSX_MIN_VERSION := -mmacosx-version-min=10.7

BUILD_TAG_DB := polinaserial_tag_db.json
BUILD_TAG_FILE := .tag

$(shell $(PYTHON) polinatag.py generate . $(BUILD_TAG_DB) > $(BUILD_TAG_FILE))

_CFLAGS := $(ARCHS)
_CFLAGS += $(MACOSX_MIN_VERSION)
_CFLAGS += -O3
_CFLAGS += -Iinclude
_CFLAGS += -MMD
_CFLAGS += -DPRODUCT_NAME=\"$(PROJ_NAME)\"
# _CFLAGS += $(CFLAGS)

_LDFLAGS := $(ARCHS)
_LDFLAGS += $(MACOSX_MIN_VERSION)
_LDFLAGS += -framework CoreFoundation
_LDFLAGS += -framework IOKit
_LDFLAGS += -sectcreate __TEXT __build_tag $(BUILD_TAG_FILE)
# _LDFLAGS += $(LDFLAGS)

SUFFIX :=

ifeq ($(CONFIG),asan)
	_CFLAGS += -fsanitize=address
	_LDFLAGS += -fsanitize=address
	SUFFIX := asan
endif

BUILD_ROOT := build
CURRENT_ROOT := $(BUILD_ROOT)/$(SUFFIX)

DIR_HELPER = mkdir -p $(@D)
GET_LOCAL_DIR = $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

ifneq ($(SUFFIX),)
	BINARY := $(BUILD_ROOT)/$(PROJ_NAME)_$(SUFFIX)
else
	BINARY := $(BUILD_ROOT)/$(PROJ_NAME)
endif

OPTIONS :=
OBJECTS :=

-include $(addsuffix /rules.mk,$(MODULES))

.PHONY: clean all

all: $(BINARY)
	@$(shell $(PYTHON) polinatag.py commit $(BUILD_TAG_DB))
	@echo "%%%%% done building"

$(BINARY): $(OBJECTS)
	@echo "\tlinking"
	@$(DIR_HELPER)
	@$(CC) $(_LDFLAGS) $^ -o $@

$(CURRENT_ROOT)/%.o: %.c
	@echo "\tbuilding C: $<"
	@$(DIR_HELPER)
	@$(CC) $(_CFLAGS) $(OPTIONS) -c $< -o $@ 

clean:
	$(shell rm -rf $(BUILD_ROOT))
	@echo "%%%%% done cleaning"

-include $(OBJECTS:.o=.d)
