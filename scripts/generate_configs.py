#!/usr/bin/env python3

"""
A tool for generating EM configs from templates.
"""

import json
import os
import sys

import validate_configs

def generateSingleRecord(record, dir_path):
    if "Type" not in record:
        return record

    if record["Type"] != "include":
        return record

    if "Path" not in record:
        print("Error: 'include' record needs 'Path' property")
        sys.exit(1)

    return validate_configs.merge_override_record(record, dir_path)

def generateSingleConfig(config, dir_path):
    records = config["Exposes"]
    appliedRecords = []

    for record in records:
        appliedRecords.append(generateSingleRecord(record, dir_path))

    newconfig = config.copy()
    newconfig["Exposes"] = appliedRecords

    return newconfig

def generateFromTemplate(filename):
    print("found template "+filename)

    gen_filename = filename.replace(".template.json", ".json")

    configs = []

    with open(filename) as fd:
        config = json.load(fd)

        if isinstance(config, dict):
            if "Exposes" not in config:
                print("Error, no 'Exposes' found in template config")
                sys.exit(1)
            configs.append(generateSingleConfig(config, os.path.dirname(filename)))

        if isinstance(config, list):
            for c in config:
                if "Exposes" not in c:
                    print("Error, no 'Exposes' found in template config")
                    sys.exit(1)
                configs.append(generateSingleConfig(c, os.path.dirname(filename)))

    print("\\- generating "+gen_filename)

    if len(configs) == 1:
        res = configs[0]
    else:
        res = configs

    with open(gen_filename, "w") as outfd:
        outfd.write(json.dumps(res, indent=4, sort_keys=True, separators=(",", ": ")))
        outfd.write("\n")

def main():
    print("Generating EM configs from templates...")

    files = []

    for root, _, filenames in os.walk("configurations"):
        for f in filenames:
            files.append(os.path.join(root, f))

    for file in files:
        if not file.endswith(".template.json"):
            continue
        generateFromTemplate(file)

if __name__ == "__main__":
    main()
