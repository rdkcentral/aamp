import re
import sys

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <logfile>")
    sys.exit(1)

logfile = sys.argv[1]

# Regex for test start, pass, fail, and disabled lines
start_re = re.compile(r"Start\s+\d+:\s+([^\r\n]+)")
pass_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?Passed\s+([0-9.]+)\s+sec")
fail_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?\*\*\*Failed\s+([0-9.]+)\s+sec")
disabled_re = re.compile(r"Test\s+#\d+:\s+([^\s]+:[^\s]+[^\s]*)\s+.*?\*\*\*Not Run \(Disabled\)\s+([0-9.]+)\s+sec")
summary_fail_re = re.compile(r"\d+\s+-\s+([^\s]+:[^\s]+[^\s]*)\s+\(Failed\)")

started = []
finished = []
tests = []
failed_tests = set()
disabled_tests = set()

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
        m_summary_fail = summary_fail_re.search(line)
        if m_summary_fail:
            failed_tests.add(m_summary_fail.group(1).strip())

# Find tests that started but did not finish
not_completed = [name for name in started if name not in finished]

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