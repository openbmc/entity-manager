#!/bin/sh

set -eu
LANG=C

json_files=$(cd configurations; find . -iname "*.json" | sort | sed 's|^\./||')

MESON_FILE=configurations/meson.build

SELF=$(basename "$0")

# Generate the Meson file
{
    echo "# This file is auto-generated. Do not edit manually."
    echo "# File content generated with ${SELF}."
    echo "configs = ["
    for file in $json_files; do
        echo "    '${file}',"
    done
    echo "]"
} > "$MESON_FILE"
