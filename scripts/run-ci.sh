#!/bin/sh

set -e

scripts/validate_configs.py -v -k -e test/expected-schema-errors.txt
scripts/check_fru_manufacturer.py