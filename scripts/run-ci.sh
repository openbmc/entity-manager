#!/bin/sh

set -eu

build_dir=${BUILD_DIR:-build}
if [ ! -f "${build_dir}/meson-private/coredata.dat" ]; then
    meson setup "${build_dir}" -Dvalidate-json=true -Dtests=disabled
else
    meson configure "${build_dir}" -Dvalidate-json=true
fi

meson compile -C "${build_dir}" check_syntax
