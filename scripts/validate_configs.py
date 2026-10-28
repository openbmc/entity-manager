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

import jsonschema.exceptions
import jsonschema.validators
import referencing
from referencing.jsonschema import DRAFT202012

DEFAULT_SCHEMA_FILENAME = "global.json"

# The D-Bus types a schema can name in "x-dbus-type", and for the integer
# ones the range a value must fall in.
DBUS_INTEGER_RANGES = {
    "byte": (0, 0xFF),
    "uint64": (0, 0xFFFFFFFFFFFFFFFF),
    "int64": (-0x8000000000000000, 0x7FFFFFFFFFFFFFFF),
}
DBUS_SCALAR_TYPES = set(DBUS_INTEGER_RANGES) | {"double", "boolean", "string"}
DBUS_ARRAY_TYPE = re.compile(r"^array\[(\w+)\]$")


def is_known_dbus_type(dbus_type):
    match = DBUS_ARRAY_TYPE.match(dbus_type)
    if match:
        dbus_type = match.group(1)
    return dbus_type in DBUS_SCALAR_TYPES


def parse_as_number(string):
    """
    Parses a string the way entity-manager does before publishing it, so a
    string that parses here is published as a number rather than a string.
    """
    base = 10
    if string.startswith("0x"):
        string = string[2:]
        base = 16
    elif string.startswith("00"):
        # zeroed out FRU fields are not seen as numbers
        return None
    if not string or not re.fullmatch(r"[0-9a-fA-F]+", string):
        return None
    try:
        return int(string, base)
    except ValueError:
        return None


def check_dbus_scalar(dbus_type, instance):
    """
    Returns why instance cannot be published as dbus_type, or None if it can.
    """
    if dbus_type == "string":
        return None if isinstance(instance, str) else "is not a string"
    if dbus_type == "boolean":
        return None if isinstance(instance, bool) else "is not a boolean"

    if isinstance(instance, str):
        # A template is only resolved once the configuration is probed.
        if "$" in instance:
            return None
        number = parse_as_number(instance)
        if number is None:
            return "is neither a number nor a template"
    elif isinstance(instance, bool) or not isinstance(instance, (int, float)):
        return "is not a number"
    else:
        number = instance

    if dbus_type == "double":
        return None
    if not isinstance(number, int):
        return "is not an integer"
    low, high = DBUS_INTEGER_RANGES[dbus_type]
    if not low <= number <= high:
        return f"is out of range for {dbus_type}"
    return None


def x_dbus_type(validator, dbus_type, instance, schema):
    """
    Checks that a value can be published as the D-Bus type its schema names,
    which the JSON type alone cannot say: a value that should be a number on
    D-Bus is often written as a string so that it can hold a template.
    """
    match = DBUS_ARRAY_TYPE.match(dbus_type)
    if match:
        if not isinstance(instance, list):
            yield jsonschema.exceptions.ValidationError(
                f"{instance!r} is not an array, as {dbus_type} needs"
            )
            return
        elements = instance
        dbus_type = match.group(1)
    else:
        elements = [instance]

    for element in elements:
        reason = check_dbus_scalar(dbus_type, element)
        if reason is not None:
            yield jsonschema.exceptions.ValidationError(
                f"{element!r} {reason}, so it cannot be published as "
                f"{dbus_type}"
            )


ConfigValidator = jsonschema.validators.extend(
    jsonschema.validators.Draft202012Validator, {"x-dbus-type": x_dbus_type}
)


def check_dbus_type_keywords(schema, location):
    """
    Rejects an "x-dbus-type" naming a type this script cannot check, which
    would otherwise let any value through.
    """
    if isinstance(schema, dict):
        dbus_type = schema.get("x-dbus-type")
        if dbus_type is not None and (
            not isinstance(dbus_type, str) or not is_known_dbus_type(dbus_type)
        ):
            raise ValueError(f"{location}: unknown x-dbus-type {dbus_type!r}")
        for key, value in schema.items():
            check_dbus_type_keywords(value, f"{location}/{key}")
    elif isinstance(schema, list):
        for index, value in enumerate(schema):
            check_dbus_type_keywords(value, f"{location}/{index}")


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
                    check_dbus_type_keywords(schema_contents, relative_uri)

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
    validator = ConfigValidator(root_schema, registry=registry)

    return validator


def is_type_mismatch(error):
    """
    Whether an error says a branch describes another kind of record, either
    by its Type or by being a different JSON type altogether.
    """
    if error.validator == "type" and not error.relative_path:
        return True
    return list(error.relative_path)[-1:] == ["Type"] or (
        error.validator in ("enum", "const")
        and list(error.relative_schema_path)[-3:-1] == ["properties", "Type"]
    )


def innermost_errors(error):
    """
    Follows a oneOf or anyOf failure into the one branch whose Type matched,
    which is the branch a record was meant for, so the errors reported are
    that branch's rather than one per record kind.
    """
    if error.validator not in ("oneOf", "anyOf") or not error.context:
        return [error]

    branches = {}
    for sub in error.context:
        branches.setdefault(sub.relative_schema_path[0], []).append(sub)
    matching = [
        subs
        for subs in branches.values()
        if not any(is_type_mismatch(sub) for sub in subs)
    ]
    if len(matching) != 1:
        return [error]
    return [inner for sub in matching[0] for inner in innermost_errors(sub)]


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
                print(f"Validation Error for {filename}: {e}")
                print("****** CONTEXT clues ******")
                for sub in e.context:
                    print(sub.message)
                print("****** Errors in the matching schema ******")
                for inner in innermost_errors(e):
                    location = "/".join(str(p) for p in inner.absolute_path)
                    print(f"{location}: {inner.message}")

    return (is_invalid, is_unexpected_pass)


if __name__ == "__main__":
    main()
