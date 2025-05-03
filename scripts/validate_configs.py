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
    schema = {}
    with open(schema_file) as fd:
        schema = json.load(fd)

    spec = jsonschema.Draft202012Validator
    spec.check_schema(schema)
    base_uri = "file://{}/".format(
        os.path.split(os.path.realpath(schema_file))[0]
    )
    resolver = jsonschema.RefResolver(base_uri, schema)
    validator = spec(schema, resolver=resolver)

    return validator


def validate_single_config(
    args, filename, config, expected_fails, schema_file
):
    expect_fail = filename in expected_fails

    is_invalid = False
    is_unexpected_pass = False

    try:
        validator = validator_from_file(schema_file)
        validator.validate(config)
        if expect_fail:
            is_unexpected_pass = True
    except jsonschema.exceptions.ValidationError as e:
        if not expect_fail:
            is_invalid = True
            if args.verbose:
                print(e)
    except FileNotFoundError:
        is_invalid = True
        if args.verbose:
            print(f"Could not read schema file: {schema_file}")

    return (is_invalid, is_unexpected_pass)


if __name__ == "__main__":
    main()
