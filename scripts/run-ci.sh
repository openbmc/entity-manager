#!/bin/sh

set -e

scripts/validate_configs.py -v -k -e test/expected-schema-errors.txt

# fail the script if someone forgot to update configurations from templates
scripts/generate_configs.py

# fail the script if someone forgot to update the list of configurations or schemas
scripts/generate_config_list.sh
scripts/generate_schema_list.sh
git --no-pager diff --exit-code -- .
