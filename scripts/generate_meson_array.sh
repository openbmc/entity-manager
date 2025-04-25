#!/bin/sh

set -eu
LANG=C

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <directory> <meson_array_name>" >&2
    exit 1
fi

dir="$1"
array_name="$2"
meson_file="${dir}/meson.build"

json_files=$(cd "$dir"; find . -iname "*.json" | sort | sed 's|^\./||')

self=$(basename "$0")

{
    echo "# This file is auto-generated. Do not edit manually."
    echo "# File content generated with ${self}."
    echo "${array_name} = ["
    for file in $json_files; do
        echo "    '${file}',"
    done
    echo "]"
} > "$meson_file"

