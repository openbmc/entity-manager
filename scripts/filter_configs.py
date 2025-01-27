#!/usr/bin/env python3

import argparse
import json

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
            if comp_hw == "*":
                return True
            elif comp_hw in compatible_hw:
                return True
            else:
                return False
        else:
            return False

    return True


def filter_configs(compatible_hw, configs):

    if len(compatible_hw) == 0:
        print("no compatible hw listed")
        return configs

    filtered_configs = []

    for config in configs:
        if filter_config(compatible_hw, config):
            filtered_configs.append(config)

    return filtered_configs


# Parse arguments
parser = argparse.ArgumentParser(
    description="Filter config files based on regex."
)
parser.add_argument(
    "--compatible-hardware",
    required=True,
    help="space separated list of hw to support, use empty list for all hw",
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
