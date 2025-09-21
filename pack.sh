#!/bin/bash

ARMV7_XCODE="/Volumes/Data/Miscellaneous/Build-environments/Xcode_armv7/Xcode.app"

set -e

mkdir -p package

echo "Cleaning"
make clean

echo "Building for Mac OS X"
WITH_TAG=1 make -j16 STYLES=RELEASE PLATFORMS=macosx

echo "Building for iPhone OS"
WITH_TAG=1 DEVELOPER_DIR=$ARMV7_XCODE WITH_ARMV7=1 make -j16 STYLES=RELEASE PLATFORMS=iphoneos

echo "Packing"
tar -cvJf package/$(cat .tag_final).tar.xz -C build iphoneos/polinaserial macosx/polinaserial ../iboot_aux_hmacs.txt

echo "Done!"
