#!/usr/bin/env python3

import argparse
import json
import re


def matches_any_regex(regexes: list[str], txt: str) -> bool:

    for reg in regexes:
        if len(re.findall(reg, txt)) > 0:
            return True
    return False


def filter_config(compatible_hw: list[str], config: str) -> bool:

    if config.endswith(".record.json"):
        return False

    try:
        with open(config) as fd:
            j = json.loads(fd)
    except Exception:
        return False

    configs = []

    # if any one config is compatible, the whole file is assumed
    # to be compatible
    if isinstance(j, list):
        for config in j:
            configs.append(config)

    if isinstance(j, dict):
        configs.append(j)

    for config in configs:
        if "CompatibleHardware" in config:
            comp_hw = config["CompatibleHardware"]
            if comp_hw == "":
                return True
            elif matches_any_regex(compatible_hw, comp_hw):
                return True
            else:
                return False
        else:
            return False

    return True


def filter_configs(compatible_hw: list[str], configs: list[str]) -> list[str]:

    if len(compatible_hw) == 0:
        return configs

    return [x for x in configs if filter_config(compatible_hw, x)]


def main():
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
        "--configs",
        required=True,
        help="Space-separated list of config files.",
    )
    args = parser.parse_args()

    configs = args.configs.split()

    filtered_configs = filter_configs(
        args.compatible_hardware.strip().split(), configs
    )

    # Print filtered configs (one per line)
    for config in filtered_configs:
        print(config)


if __name__ == "__main__":
    main()
