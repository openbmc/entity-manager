#!/bin/sh
scripts/validate-configs.py -v -k -e test/expected-schema-errors.txt
scripts/autojson.py configurations
