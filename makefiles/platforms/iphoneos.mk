CC := xcrun --sdk iphoneos clang

ifneq ($(WITH_ARMV7),1)
	ARCHS := -arch arm64
	OS_MIN_VERSION := -miphoneos-version-min=7.0
else
	ARCHS := -arch armv7 -arch arm64
	OS_MIN_VERSION := -miphoneos-version-min=6.0
endif

$(shell mkdir -p ./include-ios && ln -fsh $(shell xcrun --sdk macosx --show-sdk-path)/System/Library/Frameworks/IOKit.framework/Versions/Current/Headers ./include-ios/IOKit)

CFLAGS += -I./include-ios
OPTIONS += -DPLATFORM_IPHONEOS=1
