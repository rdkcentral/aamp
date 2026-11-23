#!/usr/bin/env bash
# Launcher for the NMQE Test Runner GUI. Needs a Python with Tk (brew install python-tk@3.14).
set -euo pipefail

cd "$(dirname "$0")"
exec python3.14 nmqe_test_runner.py "$@"
