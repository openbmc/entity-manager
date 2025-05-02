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

import jsonschema.validators

DEFAULT_SCHEMA_FILENAME = "global.json"


def get_available_cpu_count():
    """
    Get the number of CPUs available to this process,
    respecting CPU affinity masks when available.
    """
    if hasattr(os, "sched_getaffinity"):
        return len(os.sched_getaffinity(0))
    return os.cpu_count()


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


def validate_config(args):
    """
    Validate a configuration file against a schema.

    Args:
        args: Tuple containing (config_file, config, base_uri, schema, expected_fails)
            config_file: Path to the configuration file
            config: Parsed JSON configuration
            base_uri: Base URI for JSON schema references
            schema: JSON schema to validate against
            expected_fails: List of filenames expected to fail validation

    Returns:
        Dictionary with validation results
    """
    # Unpack arguments
    config_file, config, base_uri, schema, expected_fails = args

    # Create a new resolver and validator for each validation to avoid race conditions
    spec = jsonschema.Draft202012Validator
    resolver = jsonschema.RefResolver(base_uri, schema)
    validator = spec(schema, resolver=resolver)

    name = os.path.split(config_file)[1]
    expect_fail = name in expected_fails
    result = {
        "name": name,
        "file": config_file,
        "status": "valid",
        "error": None,
    }

    try:
        validator.validate(config)
        if expect_fail:
            result["status"] = "unexpected_pass"
    except jsonschema.exceptions.ValidationError as e:
        if not expect_fail:
            result["status"] = "invalid"
            result["error"] = e
        else:
            result["status"] = "expected_fail"

    return result


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
        "-t",
        "--threads",
        type=int,
        default=get_available_cpu_count(),
        help="number of threads to use for validation (default: number of available CPUs)",
    )

    parser.add_argument(
        "-v", "--verbose", action="store_true", help="be noisy"
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
            sys.stderr.write(
                "Could not guess location of {}\n".format(
                    DEFAULT_SCHEMA_FILENAME
                )
            )
            sys.exit(2)

    schema = {}
    try:
        with open(schema_file) as fd:
            schema = json.load(fd)
    except FileNotFoundError:
        sys.stderr.write(
            "Could not read schema file '{}'\n".format(schema_file)
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
            sys.stderr.write("Could not guess location of configurations\n")
            sys.exit(2)

    configs = []
    for config_file in config_files:
        try:
            with open(config_file) as fd:
                configs.append(json.loads(remove_c_comments(fd.read())))
        except FileNotFoundError:
            sys.stderr.write(
                "Could not parse config file '{}'\n".format(config_file)
            )
            sys.exit(2)

    expected_fails = []
    if args.expected_fails:
        try:
            with open(args.expected_fails) as fd:
                for line in fd:
                    expected_fails.append(line.strip())
        except Exception:
            sys.stderr.write(
                "Could not read expected fails file '{}'\n".format(
                    args.expected_fails
                )
            )
            sys.exit(2)

    spec = jsonschema.Draft202012Validator
    spec.check_schema(schema)
    base_uri = "file://{}/".format(
        os.path.split(os.path.realpath(schema_file))[0]
    )

    results = {
        "invalid": [],
        "unexpected_pass": [],
    }

    # Use ProcessPoolExecutor to validate configs in parallel
    with ProcessPoolExecutor(max_workers=args.threads) as executor:
        # Prepare arguments for each config file
        validation_args = [
            (cf, c, base_uri, schema, expected_fails)
            for cf, c in zip(config_files, configs)
        ]

        # Process configs in parallel
        validation_results = list(
            executor.map(validate_config, validation_args)
        )

    # Process results
    for result in validation_results:
        if result["status"] == "unexpected_pass":
            results["unexpected_pass"].append(result["name"])
            if not getattr(args, "continue"):
                break
        elif result["status"] == "invalid":
            results["invalid"].append(result["name"])
            if args.verbose:
                print(result["error"])
            if not getattr(args, "continue"):
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
                print("  '{}' passed!{}".format(f, unexpected_pass_suffix))
            if any([x in f for x in results["invalid"]]):
                print("  '{}' failed!".format(f))

        if show_suffix_explanation:
            print("\n** configuration expected to fail")

    sys.exit(exit_status)


if __name__ == "__main__":
    main()
