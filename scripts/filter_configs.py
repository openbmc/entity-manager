#!/usr/bin/env python3

import argparse
import json
import re


def matches_any_regex(regexes, txt):

    for re in regexes:
        if len(re.findall(re, txt)) > 0:
            return True
    return False


def filter_config(compatible_hw, config):

    if config.endswith(".record.json"):
        return True

    try:
        with open(config) as fd:
            j = json.load(fd)
    except Exception:
        return False

    if isinstance(j, dict):
        if "CompatibleHardware" in j:
            comp_hw = j["CompatibleHardware"]
            if comp_hw == "":
                return True
            elif matches_any_regex(compatible_hw, comp_hw):
                return True
            else:
                return False
        else:
            return False

    return True


def filter_configs(compatible_hw, configs):

    if len(compatible_hw) == 0:
        return configs

    filtered_configs = []

    for config in configs:
        if filter_config(compatible_hw, config):
            filtered_configs.append(config)

    return filtered_configs


# Parse arguments
parser = argparse.ArgumentParser(
    description="Filter config files based on 'CompatibleHardware' property"
)
parser.add_argument(
    "--compatible-hardware",
    required=True,
    help="space separated list of regexes for hw to support, use empty list for all hw",
)
parser.add_argument(
    "--configs", required=True, help="Space-separated list of config files."
)
args = parser.parse_args()

configs = args.configs.split()

filtered_configs = filter_configs(
    args.compatible_hardware.strip().split(), configs
)

# Print filtered configs (one per line)
for config in filtered_configs:
    print(config)
