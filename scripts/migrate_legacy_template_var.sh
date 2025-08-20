#!/bin/bash

# This script can be used to migrate JSON configurations which use old
# format for template var configuration $<template_var> to new format
# ${<template_var>}.
# USAGE:
#   ./migrate_legacy_template_var.sh <parent_directory_to_JSON_files>
# Backup files of the original configurations are renamed to *.json.bk

# Directory to search, default to current directory if not provided
DIR="${1:-.}"
find "$DIR" -type f -name '*.json' | while read -r file; do
    echo "Migrating config in file: $file"
    matches=$(grep -oP '\$[0-9a-zA-Z_]+' "$file" | sort -u)
    if [[ -n "$matches" ]]; then
        echo "$matches"
    else
        echo "No matches found."
    fi
    # Replace all occurrences of $VAR with ${VAR} using perl in-place
    # Backup files are copy to "$file".bk
    perl -i.bk -pe 's/\$([0-9a-zA-Z_]+)/\${$1}/g' "$file"
done
