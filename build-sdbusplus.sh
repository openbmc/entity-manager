#!/bin/sh
set -e
if [ ! -d $MESON_BUILD_ROOT/sdbusplus ]; then
    cp -r $MESON_SOURCE_ROOT/subprojects/sdbusplus $MESON_BUILD_ROOT
    cd $MESON_BUILD_ROOT/sdbusplus
    ./bootstrap.sh
    ./configure --enable-transaction
    make -j libsdbusplus.la
fi
