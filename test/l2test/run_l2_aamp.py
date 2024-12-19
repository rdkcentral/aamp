#!/usr/bin/env python3
# -*- coding:utf-8 -*-
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
import argparse
import os
import sys
import subprocess
import shutil
import textwrap
import json
import xml.etree.ElementTree as ET
import re


def str_to_int(in_dict):
    """
    Convert and rename specific numeric fields held as strings to ints
    """
    fields_to_convert = {'errors': 'errors', 'failures': 'failures', 'tests': 'tests', 'skipped': 'disabled'}
    out = {}
    for k in in_dict.keys():
        if k in fields_to_convert:
            out[fields_to_convert[k]] = int(in_dict[k])
        else:
            out[k] = in_dict[k]
    return out


def junit_to_json(xml_file, json_file):
    """
    Convert a junit xml file as produced by pytest into
    the googletest json format
    """
    tree = ET.parse(xml_file)
    root = tree.getroot()
    testsuites = []
    testsuite = []
    for tc in root.findall('.//testcase'):
        fails = []
        for fail in tc:
            fails.append(fail.attrib)
        xx = str_to_int(tc.attrib)
        if len(fails):
            xx.update({'failures': fails})
        testsuite.append(xx)
    ts = root.find('testsuite')
    ts_json = str_to_int(ts.attrib)
    ts_json.update({'testsuite': testsuite})
    testsuites.append(ts_json)
    tss_json = str_to_int(ts.attrib)
    tss_json.update({'testsuites': testsuites})
    final_json = {'test_cases_results': tss_json}
    with open(json_file, 'w') as f:
        f.write(json.dumps(final_json, indent=4))


def install_python_packages():
    print("install_python_packages")
    # Installing additional python test packages

    if os.path.isfile("requirements.txt"):
        print("Installing packages from requirements.txt")
        sys.stdout.flush()
        ret = subprocess.run('python3 -m pip install -r requirements.txt', shell=True)
        if ret.returncode:
            print("ERROR python3 -m pip install failed")
            exit(1)


def build_aamp(other_args):
    print("Build AAMP")
    opts = ""
    if "--coverage" in other_args:
        opts = "-c "
    if "-s" in other_args:
        opts += "-s"
    os.chdir(aampdir)
    print(os.getcwd())
    sys.stdout.flush()
    subprocess.run('bash install-aamp.sh {} -d $(pwd -P) -n || true'.format(opts), shell=True)


class Unbuffered:
    """
    Capture stdout/stderr and write it to file as well, like tee
    """
    def __init__(self, stream):
        self.stream = stream
        self.te = open('run_l2_aamp.log', 'w')  # File where you need to keep the logs

    def write(self, data):
        self.stream.write(data)
        # self.stream.flush()
        self.te.write(data)    # Write the data of stdout here to a text file as well

    def flush(self):
        pass

    def isatty(self):
        return False


sys.stdout = Unbuffered(sys.stdout)
sys.stderr = sys.stdout
print(sys.argv)
# setting file paths
l2testdir = os.getcwd()
aampdir = os.path.abspath(os.path.join('..', '..'))

epilog_txt = ""
# if not already in a venv
if 'VIRTUAL_ENV' not in os.environ:
    epilog_txt = textwrap.dedent('''\
        Suggestion: create and activate a virtual environment before running with: -
            python3 -m venv l2venv
            source l2venv/bin/activate
        ''')


# Parsing the command line inputs to get the test suite numbers to execute
# If no inputs then will execute all the test suites without making build
argParser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                    description=textwrap.dedent('''\
                                    Run L2 AAMP test(s)
                                    
                                    Additional arguments not listed in this help will be passed onto pytest
                                    
                                    examples:
                                      run_l2_aamp.py -b            # build aamp, run all tests
                                      run_l2_aamp.py               # run all tests without rebuilding
                                      run_l2_aamp.py -v            # run all tests without rebuilding, displaying video
                                      run_l2_aamp.py -t 1002 1003  # run specified tests without rebuilding
                                      run_l2_aamp.py -e 1001 2000  # run all tests excluding specified
                                    '''), epilog=epilog_txt)
argParser.add_argument("-b", "--build", help="Build aamp before running tests", action="store_true")
argParser.add_argument("-e", "--testsuites_exclude", nargs='*', help="specify test suite numbers to skip")
argParser.add_argument("-t", "--testsuites_run", nargs='*', help="specify test suite numbers to run")
argParser.add_argument("-v", "--aamp_video", help="run AAMP with video window, but no A/V gap detection",
                       action="store_true")
args, other_args = argParser.parse_known_args()

# install pip modules before trying to import them
install_python_packages()
import pytest
print("Path of AAMP:", aampdir)


# Get list of test dirs
test_suite_dirs = {}  # E.G{ 1001: 'TEXT_1001_something' ...}
test_suite_dirs_to_run = []
for directory in os.listdir(l2testdir):
    m = re.match(r'[A-Z_\-]+(\d{4,})', directory)
    if os.path.isfile(directory):
        pass
    elif m:
        num = m.group(1)
        if num in test_suite_dirs:
            print("ERROR Duplicate numbers from directories {} {} expecting unique".format
                  (test_suite_dirs[num], directory))
            exit(1)
        if args.testsuites_run:
            if num in args.testsuites_run:
                test_suite_dirs[num] = directory
        elif args.testsuites_exclude:
            if num not in args.testsuites_exclude:
                test_suite_dirs[num] = directory
        else:
            test_suite_dirs[num] = directory
    else:
        print("{} is not recognised as a directory containing tests".format(directory))
print("test_suite_dirs", test_suite_dirs)

for num in sorted(test_suite_dirs.keys()):
    test_suite_dirs_to_run.append(test_suite_dirs[num])

print("test_suite_dirs_to_run", test_suite_dirs_to_run)

# Build aamp
if args.build:
    build_aamp(other_args)

os.chdir(l2testdir)

xml_results_file = 'junit_results.xml'
json_results_file = 'L2Report.json'
# Give pytest the list of dirs
opt = ['--junitxml={}'.format(xml_results_file)] + other_args + test_suite_dirs_to_run
if args.aamp_video:
    opt.append('--aamp_video')
print(opt)
ret_code = pytest.main(opt)

print("test_suite_dirs_to_run", test_suite_dirs_to_run)
# Copy and archive the test logs under result directory
# Better if this was done by jenkins
for test_dir in test_suite_dirs_to_run:
    output = os.path.join(test_dir, 'output')
    if os.path.isdir(output):
        shutil.make_archive(os.path.join('result', test_dir), 'zip', output)

junit_to_json(xml_results_file, json_results_file)
sys.exit(ret_code)
