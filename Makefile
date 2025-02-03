#
# polinaserial makefile
#

PROJ_NAME ?= polinaserial
WITH_UART_EXTRA ?= 0

PYTHON ?= python3

CC ?= clang

ARCHS = -arch x86_64 -arch arm64
MACOSX_MIN_VERSION = -mmacosx-version-min=10.7

BUILD_TAG_DB=polinaserial_tag_db.json
BUILD_TAG_FILE=.tag

$(shell $(PYTHON) polinatag.py generate . $(BUILD_TAG_DB) > $(BUILD_TAG_FILE))

_CFLAGS += $(ARCHS)
_CFLAGS += $(MACOSX_MIN_VERSION)
_CFLAGS += -O3
_CFLAGS += -Iinclude
_CFLAGS += -MMD
_CFLAGS += -DWITH_UART_EXTRA=$(WITH_UART_EXTRA)
_CFLAGS += -DPRODUCT_NAME=\"$(PROJ_NAME)\"
_CFLAGS += $(CFLAGS)

_LDFLAGS += $(ARCHS)
_LDFLAGS += $(MACOSX_MIN_VERSION)
_LDFLAGS += -framework CoreFoundation
_LDFLAGS += -framework IOKit
_LDFLAGS += -sectcreate __TEXT __build_tag $(BUILD_TAG_FILE)
_LDFLAGS += $(LDFLAGS)


SRC_ROOT = src
BUILD_ROOT = build

SOURCES = \
	main.c \
	configuration.c \
	ll.c \
	menu.c \
	device.c \
	serial.c \
	log.c \
	lolcat.c \
	iboot.c

_SOURCES = $(addprefix $(SRC_ROOT)/, $(SOURCES))
OBJECTS = $(addprefix $(BUILD_ROOT)/, $(_SOURCES:.c=.o))

BINARY = $(BUILD_ROOT)/$(PROJ_NAME)

DIR_HELPER = mkdir -p $(@D)

.PHONY: clean all

all: $(BINARY)
	@$(shell $(PYTHON) polinatag.py commit $(BUILD_TAG_DB))
	@echo "%%%%% done building"

$(BINARY): $(OBJECTS)
	@echo "\tlinking"
	@$(DIR_HELPER)
	@$(CC) $(_LDFLAGS) $^ -o $@

$(BUILD_ROOT)/%.o: %.c
	@echo "\tbuilding C: $<"
	@$(DIR_HELPER)
	@$(CC) $(_CFLAGS) -c $< -o $@ 

clean:
	$(shell rm -rf $(BUILD_ROOT))
	@echo "%%%%% done cleaning"

-include $(OBJECTS:.o=.d)
