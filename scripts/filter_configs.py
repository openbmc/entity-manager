import argparse
import re

# Parse arguments
parser = argparse.ArgumentParser(
    description="Filter config files based on regex."
)
parser.add_argument("--regex", required=True, help="Regex pattern to match.")
parser.add_argument(
    "--configs", required=True, help="Space-separated list of config files."
)
args = parser.parse_args()

# Compile the regex
pattern = re.compile(args.regex)

# Filter configs
configs = args.configs.split()
filtered_configs = [config for config in configs if pattern.search(config)]

# Print filtered configs (one per line)
for config in filtered_configs:
    print(config)
