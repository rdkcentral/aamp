#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""List L1 tests that took longer than a given threshold.

Usage:
    ./list_slow_tests.py [OPTIONS] [XML_FILE]

Arguments:
    XML_FILE    Path to CTest JUnit XML results (default: build/ctest-results.xml)

Options:
    -t, --threshold SECONDS   Minimum duration to report (default: 1.0)
    -h, --help                Show this help message
"""

import argparse
import os
import sys
import xml.etree.ElementTree as ET


def list_slow_tests(xml_path: str, threshold: float) -> list[tuple[float, str]]:
    """Parse JUnit XML and return tests exceeding the threshold, sorted longest first."""
    tree = ET.parse(xml_path)
    slow = []
    for tc in tree.iter("testcase"):
        duration = float(tc.get("time", 0))
        if duration > threshold:
            slow.append((duration, tc.get("name", "unknown")))
    slow.sort(reverse=True)
    return slow


def main():
    parser = argparse.ArgumentParser(
        description="List L1 tests that exceeded a duration threshold."
    )
    parser.add_argument(
        "xml_file",
        nargs="?",
        default="build/ctest-results.xml",
        help="Path to CTest JUnit XML results (default: build/ctest-results.xml)",
    )
    parser.add_argument(
        "-t", "--threshold",
        type=float,
        default=1.0,
        help="Minimum duration in seconds to report (default: 1.0)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.xml_file):
        print(f"No results file found: {args.xml_file}", file=sys.stderr)
        print("Run tests first with: cd test/utests && ./run.sh", file=sys.stderr)
        sys.exit(1)

    slow = list_slow_tests(args.xml_file, args.threshold)

    if not slow:
        print(f"No tests exceeded {args.threshold:.1f}s threshold.")
        return

    print(f"\nTests that took longer than {args.threshold:.1f}s (longest first):")
    for duration, name in slow:
        print(f"  {duration:7.2f}s  {name}")
    print(f"\n  Total: {len(slow)} slow test(s)\n")


if __name__ == "__main__":
    main()
