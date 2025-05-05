#!/usr/bin/python3
# all arguments to this script are considered as json files
# and attempted to be formatted alphabetically

import json
import os
import re
import argparse
from sys import argv
from typing import List, Tuple, Union


def truncate_str(x):
    n = 50
    s = str(x)
    return (s[:n] + " ...") if len(s) > n else s


# does regex 're' match any string in str
def any_str_matches(reg, str_list):
    for s in str_list:
        if len(re.findall(reg, s)) > 0:
            return True
    return False


# @returns set of strings
def extract_names(recordList):

    names = []

    for obj in recordList:
        if "Name" in obj:
            names.append(obj["Name"])

    return set(names)


# 'opt_name' may be 'None'
# 'opt_name' is a regex for which names to consider
def common_names(mapRecords, opt_name):

    common = set(extract_names(list(mapRecords.values())[0]))

    for filename, recordList in mapRecords.items():
        common = common.intersection(extract_names(recordList))

    if opt_name != None:
        common = set(
            filter(lambda x: len(re.findall(opt_name, x)) > 0, list(common))
        )

    return common


def print_record_names(names):
    for name in sorted(names):
        print("  " + name)


# r2 is the 'common' or 'reference' record
def diff_records(r1, r2):
    extra_keys = set(r1.keys()).difference(r2.keys())
    missing_keys = set(r2.keys()).difference(r1.keys())
    same_keys = set(r1.keys()).intersection(set(r2.keys()))

    diff_keys = []
    for key in same_keys:
        if r1[key] != r2[key]:
            diff_keys.append(key)

    return extra_keys, missing_keys, same_keys, set(diff_keys)


# r2 is the 'common' or 'reference' record
def records_have_diff(r1, r2):
    extra_keys, missing_keys, same_keys, diff_keys = diff_records(r1, r2)

    if len(extra_keys) > 0 or len(missing_keys) > 0 or len(diff_keys) > 0:
        return True

    return False


# print the diff between 2 exposes records
# r2 is the 'common' or 'reference' record
# 'opt_field' may be 'None'
def print_diff_record(r1, r2, opt_field):

    extra_keys, missing_keys, same_keys, diff_keys = diff_records(r1, r2)

    extra = extra_keys.union(missing_keys)
    if len(extra) == 0 and len(diff_keys) == 0:
        return

    consider = extra.union(diff_keys)
    if opt_field != None:
        consider = consider.intersection(set({opt_field}))

    print("  " + r1["Name"])

    for key in extra_keys.intersection(consider):
        print("    + ", key, ":", truncate_str(r1[key]))

    for key in missing_keys.intersection(consider):
        print("    - ", key)

    for key in diff_keys.intersection(consider):
        print("    * ", key, ":", truncate_str(r1[key]))


def intersect_dicts(d1, d2):
    k1 = d1.keys()
    k2 = d2.keys()
    k3 = set(k1).intersection(set(k2))

    res = {}
    # only add keys for which both dicts have the same value
    for key in k3:
        if d1[key] == d2[key]:
            res[key] = d1[key]

    return res


# 'mapRecords' is map of filename to list of EM exposes records
def create_common_records_map(mapRecords):

    common = common_names(mapRecords, None)

    common_records_map = {}

    for com_name in common:
        for _, records in mapRecords.items():
            # records = mapRecords[[k for k in mapRecords.keys()][0]]
            for record in records:
                if record["Name"] == com_name:
                    if com_name not in common_records_map:
                        common_records_map[com_name] = record
                    else:
                        common_records_map[com_name] = intersect_dicts(
                            record, common_records_map[com_name]
                        )

    return common_records_map


# returns True if config has a diff in some record which is common among all input configs
# 'opt_name' may be 'None'
# when 'opt_name' is provided, the diff is only considered if 'opt_name' is present in record
def config_has_diff(records, common_records_map, opt_name):
    common_names = common_records_map.keys()

    for record in records:
        name = record["Name"]

        if opt_name != None and len(re.findall(opt_name, name)) == 0:
            continue

        if name in common_names:
            if records_have_diff(record, common_records_map[name]):
                return True
    return False


# 'opt_name' may be 'None' in case no specific record
# 'opt_field' may be 'None' in case no specific field
def print_common_records_with_diff_inner(
    filename, records, mapRecords, common_records_map, opt_name, opt_field
):

    common = common_names(mapRecords, opt_name)

    if not config_has_diff(records, common_records_map, opt_name):
        return

    print(filename + ":")

    for record in records:
        name = record["Name"]
        if name in common:
            print_diff_record(record, common_records_map[name], opt_field)


# 'opt_name' may be 'None' in case no specific record
# 'opt_field' may be 'None' in case no specific field
def print_common_records_with_diff(
    mapRecords, common_records_map, opt_name, opt_field
):

    print("\nCommon Records with diff:")
    print("Legend:")
    print(" + : Extra Key")
    print(" - : Missing Key (compared to common set)")
    print(" * : Different Value")
    for filename, records in mapRecords.items():
        print_common_records_with_diff_inner(
            filename,
            records,
            mapRecords,
            common_records_map,
            opt_name,
            opt_field,
        )


# 'opt_name' may be 'None' in case no specific record
# 'opt_field' may be 'None' in case of looking at all properties of records
def diff_configs(files, common_only, diff_only, opt_name, opt_field):

    mapRecords = map_records(files)

    common_records_map = create_common_records_map(mapRecords)

    if not diff_only:
        print("\nCommon Records:")
        print_record_names(common_names(mapRecords, opt_name))

    if common_only:
        return

    common = common_names(mapRecords, opt_name)

    print("\nNon-common Records:")
    for filename, config in mapRecords.items():
        names = extract_names(config)
        extra_names = names.difference(common)

        if opt_name != None and not any_str_matches(opt_name, extra_names):
            continue

        if len(extra_names) > 0:
            print(filename + ":")
            print_record_names(extra_names)

    print_common_records_with_diff(
        mapRecords, common_records_map, opt_name, opt_field
    )


def map_records(files):

    # map config filename to list of records
    mapRecords = {}

    for file in files:
        if not file.endswith(".json"):
            continue

        with open(file) as fp:
            j = json.loads(fp.read())

        if not file.endswith(".record.json"):
            mapRecords[file] = []

            if isinstance(j, list):
                for item in j:
                    for record in item["Exposes"]:
                        mapRecords[file].append(record)
            else:
                for record in j["Exposes"]:
                    mapRecords[file].append(record)

    return mapRecords


def is_valid_full_config_filename(file):

    if not file.endswith(".json"):
        return False
    if file.endswith(".template.json"):
        return False
    if file.endswith(".record.json"):
        return False
    return True


def main():

    # Parse arguments
    parser = argparse.ArgumentParser(
        description="Compare config files for differences"
    )
    parser.add_argument(
        "-c", "--common", type=bool, help="Only show common records"
    )
    parser.add_argument(
        "-d", "--diff", type=bool, help="Only show diff between records"
    )
    parser.add_argument(
        "-n",
        "--name",
        type=str,
        help="Only show output for record 'Name's matching this regex",
    )
    parser.add_argument(
        "-f",
        "--field",
        type=str,
        help="Which property to look at for diff. No argument means all properties.",
    )
    parser.add_argument(
        "configs",
        type=str,
        nargs="+",
        help="Space-separated list of config files.",
    )
    args = parser.parse_args()

    files = []

    for file in args.configs:
        if not os.path.isdir(file):
            files.append(file)
            continue
        for root, _, filenames in os.walk(file):
            for f in filenames:
                files.append(os.path.join(root, f))

    full_configs = []
    for file in files:
        if is_valid_full_config_filename(file):
            full_configs.append(file)

    diff_configs(full_configs, args.common, args.diff, args.name, args.field)


if __name__ == "__main__":
    main()
