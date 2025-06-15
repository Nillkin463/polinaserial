#
# polinaserial makefile
#

PROJ_NAME ?= polinaserial

VALID_STYLES	:=	RELEASE ASAN PROFILING
VALID_PLATFORMS	:=	macosx iphoneos

STYLES		?=	$(VALID_STYLES)
MAKE_STYLES	=	$(filter $(VALID_STYLES), $(STYLES))

PLATFORMS		?=	$(VALID_PLATFORMS)
MAKE_PLATFORMS	=	$(filter $(VALID_PLATFORMS), $(PLATFORMS))

LIST = $(foreach platform,$(MAKE_PLATFORMS),$(addprefix $(platform)-,$(MAKE_STYLES)))

DIRTY := $(shell git diff-files --quiet; if [ $$? ]; then echo -dirty; fi)

BUILD_TAG_BASE_FILE := .tag
BUILD_TAG_FILE 		:= .tag_final

ifeq ($(WITH_TAG),1)
    $(shell printf "%s%s" $(shell cat $(BUILD_TAG_BASE_FILE)) $(DIRTY) > $(BUILD_TAG_FILE))
else
    $(shell printf "%s-%s%s" $(PROJ_NAME) $(shell TZ=UTC date +"%Y-%m-%dT%H:%M:%S") $(DIRTY) > $(BUILD_TAG_FILE))
endif

BUILD_ROOT := build

all:	$(LIST)

$(LIST):	platform	=	$(word 1, $(subst -, ,$@))
$(LIST):	style		=	$(word 2, $(subst -, ,$@))
$(LIST):
	@echo %%% building $(platform)-$(style)
	@$(MAKE) -f makefiles/main.mk PLATFORM=$(platform) STYLE=$(style) BUILD_ROOT=$(BUILD_ROOT) BUILD_TAG_FILE=$(BUILD_TAG_FILE) PROJ_NAME=$(PROJ_NAME)

clean:
	$(shell rm -rf $(BUILD_ROOT))
	@echo "%%%%% done cleaning"
