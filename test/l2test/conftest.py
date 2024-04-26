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
import glob
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
    parser.addoption("--sim_log", action="store_true", help="AAMP logging to stdout rather than file")
    parser.addoption("--coverage", action="store_true", help="Record AAMP line coverage when running tests")


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


coverage_info=[]
aamp_home = framework.get_aamp_home()
# aamp .gcno files get put in different location depending on mac or linux build
# need to find where that is
location_of_gcno_files = None


def collect_coverage_after_test(test_dir_path):
    print("collect_coverage_after_test")

    unique_test_filename = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    coverage_file_name = unique_test_filename + ".info"

    cmd = ["lcov", "--directory", location_of_gcno_files, "--capture",
           "--exclude", "/usr/*",
           "--output-file", test_dir_path + "/" + coverage_file_name]
    print(cmd)
    ret = subprocess.run(cmd)
    if ret.returncode != 0:
        print(ret.stdout,ret.stderr)
    assert ret.returncode == 0, "{} failed".format(cmd)
    coverage_info.append(test_dir_path + "/" + coverage_file_name)


def do_coverage_init():
    print("do_coverage_init")
    global location_of_gcno_files
    # Either AampConfig.cpp.gcno AampConfig.gcno depending on OS
    file = glob.glob(aamp_home+"/build/**/AampConfig*.gcno",recursive=True)
    assert len(file) == 1, "Cannot find AampConfig*.gcno. Did aamp get compiled for coverage? {}".format(len(file))
    location_of_gcno_files = os.path.dirname(file[0])
    cmd = ["lcov", "--zerocounters", "--directory", location_of_gcno_files]
    print(cmd)
    ret = subprocess.run(cmd)
    assert ret.returncode == 0, "{} failed".format(cmd)


def do_coverage_total():
    print("do_coverage_total")
    cmd = ["genhtml", "--demangle-cpp", "-o", "Coverage"] + coverage_info
    print(cmd)
    ret = subprocess.run(cmd)
    if ret.returncode:
        print(ret.stderr)
    assert ret.returncode == 0, "{} failed".format(cmd)


@pytest.fixture(scope="session", autouse=True)
def all_finished(request):
    if request.config.getoption('coverage'):
        do_coverage_init()
        request.addfinalizer(do_coverage_total)

