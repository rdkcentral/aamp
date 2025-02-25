# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
#
import pytest
import framework
import subprocess
import os
import sys
import glob
from http_server import HttpServerThread
# The following ensures pip installs the package although we invoke it from
# the command line and not via python
import lcov_cobertura

"""
A good starting point
https://stackoverflow.com/questions/34466027/what-is-conftest-py-for-in-pytest
"""


def pytest_addoption(parser):
    """
    Add some options to pytest that can then be passed
    onto framework.py that is used for testing aamp
    """
    parser.addoption("--aamp_video", action="store_true", help="run AAMP with video window")
    parser.addoption("--aamp_log", action="store_true", help="AAMP logging to stdout rather than file")
    parser.addoption("--coverage", action="store_true", help="Record AAMP line coverage when running tests")
    parser.addoption("--slow_tests", action="store_true", default=False, help="Include running of slow tests")

#https://docs.pytest.org/en/latest/example/simple.html#control-skipping-of-tests-according-to-command-line-option
def pytest_configure(config):
    config.addinivalue_line("markers", "slow: mark test as slow to run")

def pytest_collection_modifyitems(config, items):
    if config.getoption("--slow_tests"):
        # --slow_tests given in cli: do not skip slow tests
        return
    slow_tests = pytest.mark.skip(reason="--slow_tests option to run")
    for item in items:
        if "slow" in item.keywords:
            item.add_marker(slow_tests)

@pytest.fixture(scope="session")
def http_port():
    # Allow a user to choose a different port, if 8080 is already in use on their computer
    return os.environ["L2_HTTP_PORT"] if "L2_HTTP_PORT" in os.environ else "8080"

@pytest.fixture(scope="module")
def httpserver_setup_teardown(http_port):
	server_thread = HttpServerThread(int(http_port))
	server_thread.start()
	yield server_thread.httpd.server_address
	server_thread.stop()

@pytest.fixture
def aamp_setup_teardown(request):
    """
    The following creates an aamp instance before the test starts
    and ensures that any running aamp or simlinear processes are
    stopped when the test finishes
    """
    aamp = framework.Aamp(request)
    yield aamp
    print("Cleanup")
    aamp.exit_aamp()
    if request.config.getoption('coverage'):
        collect_coverage_after_test(aamp.test_dir_path)


# Get the current directory
current_dir = os.path.dirname(os.path.abspath(__file__))

# Add the utilities subdirectory
util_subdirectory = 'utilities'

# Construct the full path to the utilities subdirectory
util_subdirectory_path = os.path.join(current_dir, util_subdirectory)

# Add the subdirectory to sys.path
if util_subdirectory_path not in sys.path:
    sys.path.append(util_subdirectory_path)

coverage_info=[]
aamp_home = framework.get_aamp_home()
# aamp .gcno files get put in different location depending on mac or linux build
# need to find where that is
location_of_gcno_files = None


def run_cmd(cmd):
    """
    Run a command in a subprocess and check that it returns a zero (ok)
    exit code
    """
    print(cmd)
    ret = subprocess.run(cmd)
    if ret.returncode:
        print(ret.stderr)
    assert ret.returncode == 0, "{} failed".format(cmd)


def collect_coverage_after_test(test_dir_path):
    """
    Run after each test to collect the coverage data for that test
    """
    print("collect_coverage_after_test")

    unique_test_filename = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    coverage_file_name = unique_test_filename + ".info"

    cmd = ["lcov", "--directory", location_of_gcno_files, "--capture",
           "--exclude", "/usr/*",
           "--output-file", test_dir_path + "/" + coverage_file_name]
    run_cmd(cmd)

    coverage_info.append(test_dir_path + "/" + coverage_file_name)


def do_coverage_init():
    """
    Run before any tests when code coverage is enabled to
    Initialize coverage parameters
    """
    print("do_coverage_init")
    global location_of_gcno_files
    # Either AampConfig.cpp.gcno AampConfig.gcno depending on OS
    file = glob.glob(aamp_home+"/build/**/AampConfig*.gcno",recursive=True)
    assert len(file) == 1, "Cannot find AampConfig*.gcno. Did aamp get compiled for coverage? {}".format(len(file))
    location_of_gcno_files = os.path.dirname(file[0])
    cmd = ["lcov", "--zerocounters", "--directory", location_of_gcno_files]
    run_cmd(cmd)


def do_coverage_total():
    """
    Run after all tests to accumulate total coverage data from
    data for each test
    """
    print("do_coverage_total")

    cmd = ["lcov", "--output-file", "everything.info"]
    for f in coverage_info:
        cmd.append("--add-tracefile")
        cmd.append(f)
    run_cmd(cmd)

    cmd = ["genhtml", "--demangle-cpp", "-o", "Coverage", "everything.info"]
    run_cmd(cmd)

    # Generate coverage.xml
    cmd = ["lcov_cobertura", "everything.info", "-b", aamp_home, "--demangle"]
    run_cmd(cmd)


@pytest.fixture(scope="session", autouse=True)
def all_finished(request):
    if request.config.getoption('coverage'):
        do_coverage_init()
        request.addfinalizer(do_coverage_total)
