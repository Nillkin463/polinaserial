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

BUILD_TAG_BASE_FILE := .tag
DIRTY := $(shell git diff-files --quiet; echo $$?)

ifeq ($(DIRTY),0)
	BUILD_TAG_FILE := $(BUILD_TAG_BASE_FILE)
else
    $(shell printf $(shell cat $(BUILD_TAG_BASE_FILE))-dirty > .tag_final)
	BUILD_TAG_FILE := .tag_final
endif

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
	CFLAGS += -gfull
	LDFLAGS += -gfull
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

clean:
	$(shell rm -rf $(BUILD_ROOT))
	@echo "%%%%% done cleaning"

-include $(OBJECTS:.o=.d)
