#!/bin/sh

set -eu

self=$(basename "$0")

./scripts/generate_meson_array.sh configurations configs "${self}"
