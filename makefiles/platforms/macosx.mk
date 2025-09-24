CC := xcrun --sdk macosx clang
ARCHS := -arch x86_64 -arch arm64
OS_MIN_VERSION := -mmacosx-version-min=10.7

OPTIONS += -DPLATFORM_MACOSX=1
