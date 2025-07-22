#!/usr/bin/env python3
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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
"""
Google Test Log Parser

This script parses AAMP L1 Google Test (gtest) log result file and provides a
comprehensive analysis of test results. It categorizes tests into different
states and provides timing information.

Features:
- Identifies passed, failed, disabled, and skipped tests
- Detects tests that started but never completed (incomplete tests)
- Finds tests with unknown/unrecognized result status
- Provides timing analysis showing slowest to fastest tests
- Displays summary statistics including total tests and execution time

Usage:
    python3 parse_gtest_log.py <logfile>

Output sections:
- Failed tests: Tests that explicitly failed
- Incomplete tests: Tests that started but have no result line
- Test timing: All tests ordered by execution time (slowest first)
- Disabled tests: Tests marked as disabled or skipped
- Unknown results: Tests with unrecognized result status
"""

import re
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <logfile>")
    sys.exit(1)

logfile = sys.argv[1]

# Regex for test start, pass, fail, disabled, and skipped lines
start_re = re.compile(r"Start\s+\d+:\s+([^\r\n]+)")
pass_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?Passed\s+([0-9.]+)\s+sec")
fail_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?\*\*\*Failed\s+([0-9.]+)\s+sec")
disabled_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?\*\*\*Not Run \(Disabled\)\s+([0-9.]+)\s+sec")
skipped_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?\*\*\*Skipped")
summary_fail_re = re.compile(r"\d+\s+-\s+([^\s]+:[^\s]+[^\s]*)\s+\(Failed\)")

started = []
finished = []
tests = []
failed_tests = set()
disabled_tests = set()
unknown_tests = set()

with open(logfile, encoding="utf-8") as f:
    for line in f:
        m_start = start_re.search(line)
        if m_start:
            started.append(m_start.group(1).strip())
        m_pass = pass_re.search(line)
        if m_pass:
            name = m_pass.group(1).strip()
            duration = float(m_pass.group(2))
            finished.append(name)
            tests.append((name, duration, "Passed"))
        m_fail = fail_re.search(line)
        if m_fail:
            name = m_fail.group(1).strip()
            duration = float(m_fail.group(2))
            finished.append(name)
            tests.append((name, duration, "Failed"))
            failed_tests.add(name)
        m_disabled = disabled_re.search(line)
        if m_disabled:
            name = m_disabled.group(1).strip()
            duration = float(m_disabled.group(2))
            finished.append(name)
            tests.append((name, duration, "Disabled"))
            disabled_tests.add(name)
        m_skipped = skipped_re.search(line)
        if m_skipped:
            name = m_skipped.group(1).strip()
            finished.append(name)
            tests.append((name, 0.0, "Disabled"))  # Treat skipped tests as disabled
            disabled_tests.add(name)
        m_summary_fail = summary_fail_re.search(line)
        if m_summary_fail:
            failed_tests.add(m_summary_fail.group(1).strip())

# Check for tests that finished but with unknown status
known_tests = set()
for name, duration, status in tests:
    known_tests.add(name)

# Find tests that started but did not finish with a known status
not_completed = [name for name in started if name not in known_tests]

# Find tests that finished but with unknown status
for name in finished:
    if name not in known_tests:
        tests.append((name, 0.0, "Unknown"))
        unknown_tests.add(name)

total_tests = len(tests)
total_time = sum(d for _, d, _ in tests)
tests_sorted = sorted(tests, key=lambda x: -x[1])

print(f"Total tests: {total_tests}")
print(f"Total time: {total_time:.2f} sec\n")

print("Tests that FAILED:")
if failed_tests:
    for name in sorted(failed_tests):
        print(f"  {name}")
else:
    print("  None")
print()

print("Tests that started but did NOT complete:")
if not_completed:
    for name in not_completed:
        print(f"  {name}")
else:
    print("  None")
print()

print("Tests ordered from slowest to fastest:")
for name, duration, status in tests_sorted:
    print(f"{duration:7.2f} sec  {name} [{status}]")

print()
print("Tests that were DISABLED:")
if disabled_tests:
    for name in sorted(disabled_tests):
        print(f"  {name}")
else:
    print("  None")

print()
print("Tests with UNKNOWN result:")
if unknown_tests:
    for name in sorted(unknown_tests):
        print(f"  {name}")
else:
    print("  None")