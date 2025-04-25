#!/bin/sh

set -eu
LANG=C

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <directory> <meson_array_name> <script_name>" >&2
    exit 1
fi

dir="$1"
array_name="$2"
script_name="$3"
meson_file="${dir}/meson.build"

json_files=$(cd "$dir"; find . -regex "[\./]?[\/a-zA-Z0-9_\-]+.json" | sort | sed 's|^\./||')

{
    echo "# This file is auto-generated. Do not edit manually."
    echo "# File content generated with ${script_name}"
    echo "${array_name} = ["
    for file in $json_files; do
        echo "    '${file}',"
    done
    echo "]"
} > "$meson_file"

