#!/bin/sh

set -eu

json_files=$(cd configurations; find . -iname "*.json")

MESON_FILE=configurations/meson.build

# Generate the Meson file
{
    echo "# This file is auto-generated. Do not edit manually."
    echo "configs = ["
    for file in $json_files; do
        echo "  '${file}',"
    done
    echo "]"
} > "$MESON_FILE"
