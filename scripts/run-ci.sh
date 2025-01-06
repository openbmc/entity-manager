#!/bin/sh

set -e

scripts/validate_configs.py -v -k -e test/expected-schema-errors.txt

# fail the script if someone forgot to update the list of configurations
cp configurations/meson.build /tmp/configurations_meson.txt
scripts/generate_config_list.sh
diff /tmp/configurations_meson.txt configurations/meson.build
