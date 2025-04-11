#!/usr/bin/python3
# all arguments to this script are considered as json files
# and attempted to be formatted alphabetically

import json
import os
import re
from sys import argv
from typing import List, Tuple, Union

# Trying to parse JSON comments and then being able to re-insert them into
# the correct location on a re-emitted and sorted JSON would be very difficult.
# To make this somewhat manageable, we take a few shortcuts here:
#
#       - Single-line style comments (//) can be on a new line or at the end of
#         a line with contents.
#
#       - Multi-line style comments (/* */) use the must be free-standing.
#
#       - Comments will get inserted back into the file in the line they came
#         from.  If keys are resorted or the number of lines change, all bets
#         for correctness are off.
#
#       - No attempts to re-indent multi-line comments will be made.
#
# In light of this, it is highly recommended to use a JSON formatter such as
# prettier before using this script and planning to move multi-line comments
# around after key resorting.


class CommentTracker:
    # Regex patterns used.
    single_line_pattern = re.compile(r"\s*//.*$")
    multi_line_start_pattern = re.compile(r"/\*")
    multi_line_end_pattern = re.compile(r".*\*/", re.MULTILINE | re.DOTALL)

    def __init__(self) -> None:
        self.comments: List[Tuple[bool, int, str]] = []

    # Extract out the comments from a JSON-like string and save them away.
    def extract_comments(self, contents: str) -> str:
        result = []

        multi_line_segment: Union[str, None] = None
        multi_line_start = 0

        for idx, line in enumerate(contents.split("\n")):
            single = CommentTracker.single_line_pattern.search(line)
            if single:
                do_append = False if line.startswith(single.group(0)) else True
                line = line[: single.start(0)]
                self.comments.append((do_append, idx, single.group(0)))

            multi_start = CommentTracker.multi_line_start_pattern.search(line)
            if not multi_line_segment and multi_start:
                multi_line_start = idx
                multi_line_segment = line
            elif multi_line_segment:
                multi_line_segment = multi_line_segment + "\n" + line

            if not multi_line_segment:
                result.append(line)
                continue

            multi_end = CommentTracker.multi_line_end_pattern.search(
                multi_line_segment
            )
            if multi_end:
                self.comments.append(
                    (False, multi_line_start, multi_end.group(0))
                )
                result.append(multi_line_segment[multi_end.end(0) :])
                multi_line_segment = None

        return "\n".join(result)

    # Re-insert the saved off comments into a JSON-like string.
    def insert_comments(self, contents: str) -> str:
        result = contents.split("\n")

        for append, idx, string in self.comments:
            if append:
                result[idx] = result[idx] + string
            else:
                result = result[:idx] + string.split("\n") + result[idx:]

        return "\n".join(result)


files = []

for file in argv[1:]:
    if not os.path.isdir(file):
        files.append(file)
        continue
    for root, _, filenames in os.walk(file):
        for f in filenames:
            files.append(os.path.join(root, f))

for file in files:
    if not file.endswith(".json"):
        continue
    print("formatting file {}".format(file))

    comments = CommentTracker()

    with open(file) as fp:
        j = json.loads(comments.extract_comments(fp.read()))

    if not file.endswith(".record.json"):
        if isinstance(j, list):
            for item in j:
                item["Exposes"] = sorted(
                    item["Exposes"], key=lambda k: k["Type"]
                )
        else:
            j["Exposes"] = sorted(j["Exposes"], key=lambda k: k["Type"])

    with open(file, "w") as fp:
        contents = json.dumps(
            j, indent=4, sort_keys=True, separators=(",", ": ")
        )

        fp.write(comments.insert_comments(contents))
        fp.write("\n")
