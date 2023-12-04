#!/usr/bin/python3
# all arguments to this script are considered as json files
# and attempted to be formatted alphabetically

import json
import os
import re
from sys import argv


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


files = argv[1:]

for file in files[:]:
    if os.path.isdir(file):
        files.remove(file)
        for f in os.listdir(file):
            files.append(os.path.join(file, f))

for file in files:
    if not file.endswith(".json"):
        continue
    print("formatting file {}".format(file))
    with open(file) as f:
        j = json.loads(remove_c_comments(f.read()))

    if isinstance(j, list):
        for item in j:
            item["Exposes"] = sorted(item["Exposes"], key=lambda k: k["Type"])
    else:
        j["Exposes"] = sorted(j["Exposes"], key=lambda k: k["Type"])

    with open(file, "w") as f:
        f.write(
            json.dumps(j, indent=4, sort_keys=True, separators=(",", ": "))
        )
        f.write("\n")
