#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
A tool for validating entity manager configurations.
"""

import argparse
import json
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import jsonschema.exceptions
import jsonschema.validators
import referencing
from referencing.jsonschema import DRAFT202012

DEFAULT_SCHEMA_FILENAME = "global.json"
DEFAULT_SCHEMA_FILENAME_RECORD = "exposes_record.json"


def get_default_thread_count() -> int:
    """
    Returns the number of CPUs available to the current process.
    """
    try:
        # This will respect CPU affinity settings
        return len(os.sched_getaffinity(0))
    except AttributeError:
        # Fallback for systems without sched_getaffinity
        return os.cpu_count() or 1


def remove_c_comments(string):
    # first group captures quoted strings (double or single)
    # second group captures comments (//single-line or /* multi-line */)
    pattern = r"(\".*?(?<!\\)\"|\'.*?(?<!\\)\')|(/\*.*?\*/|//[^\r\n]*$)"
    regex = re.compile(pattern, re.MULTILINE | re.DOTALL)

    def _replacer(match):
        if match.group(2) is not None:
            return ""
        else:
            return match.group(1)

    return regex.sub(_replacer, string)


def main():
    parser = argparse.ArgumentParser(
        description="Entity manager configuration validator",
    )
    parser.add_argument(
        "-s",
        "--schema",
        help=(
            "Use the specified schema file instead of the default "
            "(__file__/../../schemas/global.json)"
        ),
    )
    parser.add_argument(
        "-c",
        "--config",
        action="append",
        help=(
            "Validate the specified configuration files (can be "
            "specified more than once) instead of the default "
            "(__file__/../../configurations/**.json)"
        ),
    )
    parser.add_argument(
        "-e",
        "--expected-fails",
        help=(
            "A file with a list of configurations to ignore should "
            "they fail to validate"
        ),
    )
    parser.add_argument(
        "-k",
        "--continue",
        action="store_true",
        help="keep validating after a failure",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="be noisy"
    )
    parser.add_argument(
        "-t",
        "--threads",
        type=int,
        default=get_default_thread_count(),
        help="Number of threads to use for parallel validation (default: number of CPUs)",
    )
    args = parser.parse_args()

    schema_file = args.schema
    if schema_file is None:
        try:
            source_dir = os.path.realpath(__file__).split(os.sep)[:-2]
            schema_file = os.sep + os.path.join(
                *source_dir, "schemas", DEFAULT_SCHEMA_FILENAME
            )
        except Exception:
            print(
                f"Could not guess location of {DEFAULT_SCHEMA_FILENAME}",
                file=sys.stderr,
            )
            sys.exit(2)

    config_files = args.config or []
    if len(config_files) == 0:
        try:
            source_dir = os.path.realpath(__file__).split(os.sep)[:-2]
            configs_dir = os.sep + os.path.join(*source_dir, "configurations")
            data = os.walk(configs_dir)
            for root, _, files in data:
                for f in files:
                    if f.endswith(".json"):
                        config_files.append(os.path.join(root, f))
        except Exception:
            print(
                "Could not guess location of configurations", file=sys.stderr
            )
            sys.exit(2)

    configs = []
    for config_file in config_files:
        try:
            with open(config_file) as fd:
                configs.append(json.loads(remove_c_comments(fd.read())))
        except FileNotFoundError:
            print(
                f"Could not parse config file: {config_file}", file=sys.stderr
            )
            sys.exit(2)

    expected_fails = []
    if args.expected_fails:
        try:
            with open(args.expected_fails) as fd:
                for line in fd:
                    expected_fails.append(line.strip())
        except Exception:
            print(
                f"Could not read expected fails file: {args.expected_fails}",
                file=sys.stderr,
            )
            sys.exit(2)

    results = {
        "invalid": [],
        "unexpected_pass": [],
    }

    should_continue = getattr(args, "continue")

    with ProcessPoolExecutor(max_workers=args.threads) as executor:
        # Submit all validation tasks
        config_to_future = {}
        for config_file, config in zip(config_files, configs):
            filename = os.path.split(config_file)[1]
            future = executor.submit(
                validate_single_config,
                args,
                filename,
                config,
                expected_fails,
                schema_file,
                source_dir,
                config_file,
            )
            config_to_future[config_file] = future

        # Process results as they complete
        for config_file, future in config_to_future.items():
            # Wait for the future to complete and get its result
            is_invalid, is_unexpected_pass = future.result()
            # Update the results with the validation result
            filename = os.path.split(config_file)[1]
            if is_invalid:
                results["invalid"].append(filename)
            if is_unexpected_pass:
                results["unexpected_pass"].append(filename)

            # Stop validation if validation failed unexpectedly and --continue is not set
            validation_failed = is_invalid or is_unexpected_pass
            if validation_failed and not should_continue:
                executor.shutdown(wait=False, cancel_futures=True)
                break

    exit_status = 0
    if len(results["invalid"]) + len(results["unexpected_pass"]):
        exit_status = 1
        unexpected_pass_suffix = " **"
        show_suffix_explanation = False
        print("results:")
        for f in config_files:
            if any([x in f for x in results["unexpected_pass"]]):
                show_suffix_explanation = True
                print(f"  '{f}' passed!{unexpected_pass_suffix}")
            if any([x in f for x in results["invalid"]]):
                print(f"  '{f}' failed!")

        if show_suffix_explanation:
            print("\n** configuration expected to fail")

    sys.exit(exit_status)


def validator_from_file(schema_file):
    # Get root directory of schema file, so we can walk all the directories
    # for referenced schemas.
    schema_path = os.path.dirname(schema_file)

    root_schema = None
    registry = referencing.Registry()

    # Pre-load all .json files from the schemas directory and its subdirectories
    # into the registry. This allows $refs to resolve to any schema.
    for dirpath, _, directory in os.walk(schema_path):
        for filename in directory:
            if filename.endswith(".json"):
                full_file_path = os.path.join(dirpath, filename)

                # The URI  is their path relative to schema_path.
                relative_uri = os.path.relpath(full_file_path, schema_path)

                with open(full_file_path, "r") as fd:
                    schema_contents = json.loads(remove_c_comments(fd.read()))
                    jsonschema.validators.Draft202012Validator.check_schema(
                        schema_contents
                    )

                    # Add to the registry.
                    registry = registry.with_resource(
                        uri=relative_uri,
                        resource=referencing.Resource.from_contents(
                            schema_contents, default_specification=DRAFT202012
                        ),
                    )

                    # If this was the schema_file we need to save the contents
                    # as the root schema.
                    if schema_file == full_file_path:
                        root_schema = schema_contents

    # Create the validator instance with the schema content and the configured registry.
    validator = jsonschema.validators.Draft202012Validator(
        root_schema, registry=registry
    )

    return validator


def validate_single_config(
    args,
    filename,
    config,
    expected_fails,
    schema_file,
    source_dir,
    config_file,
):
    expect_fail = filename in expected_fails
    name = os.path.split(filename)[1]

    is_invalid = False
    is_unexpected_pass = False

    schema_filename_record = os.sep + os.path.join(
        *source_dir, "schemas", DEFAULT_SCHEMA_FILENAME_RECORD
    )
    validator_record = validator_from_file(schema_filename_record)

    try:
        validator = validator_from_file(schema_file)
        validator.validate(config)
        if expect_fail:
            is_unexpected_pass = True
    except jsonschema.exceptions.ValidationError as e:
        if not expect_fail:
            is_invalid = True
            if args.verbose:
                print(f"Validation Error for {filename}: {e}")

    if is_invalid:
        return (is_invalid, is_unexpected_pass)

    # in case of EM record json, no override validation
    if isinstance(config, dict):
        if "Exposes" not in config:
            return (is_invalid, is_unexpected_pass)

    records = collect_exposes_records(config)

    for record in records:
        if record["Type"] is not None and record["Type"] == "include":
            if args.verbose:
                print("include / override detected in", name)

            if not validate_override_record(
                record, validator_record, Path(config_file).parent, args
            ):
                if not expect_fail:
                    is_invalid = True
                    return (is_invalid, is_unexpected_pass)

    return (is_invalid, is_unexpected_pass)


def merge_override_record(record, base_dir):

    path = record["Path"]

    with open(os.path.join(base_dir, path)) as fd:
        res = json.loads(fd.read())

    if "Override" in record:
        overrides = record["Override"]

        for key in overrides.keys():
            res[key] = overrides[key]

    return res


def validate_override_record(record, validator_record, base_dir, args):

    if "Path" not in record:
        print("missing 'Path' in override record")

    merged_config = merge_override_record(record, base_dir)

    try:
        validator_record.validate(merged_config)
    except jsonschema.exceptions.ValidationError as e:
        if args.verbose:
            print(e)
        return False

    return True


def collect_exposes_records(config):
    records = []

    if isinstance(config, dict):
        records = config["Exposes"]

    if isinstance(config, list):
        for item in config:
            exposes = item["Exposes"]
            for record in exposes:
                records.append(record)

    return records


if __name__ == "__main__":
    main()
