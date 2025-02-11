#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events


import os
import json
import pytest
from inspect import getsourcefile

DASH_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/DELIA-65821_Multiperiod/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd"

DASH_TEST_STREAM_1 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-TUNE-1007_AdInsertion/charter_dai/tl_dai.mpd"

TESTS = [{"cmd": 'setconfig {"info":true,"progress":true,"jsinfo":true}'}]

# Create instances
playback = {}

# Function to check if Skipping Fetchfragment logline from regular expression match
def check_skipFetchFragment(re_match):
    current_directory = os.getcwd()
    folder_name = 'AAMP-TUNE-1007_AdInsertion/output/'
    file_name = 'test_1007[test_data0].log'
    logfile_path = os.path.join(current_directory, folder_name, file_name)
    with open(logfile_path, 'r') as file:
        log_content = file.read()
        assert "Skipping Fetchfragment" not in log_content, "Test failed: 'Skipping Fetchfragment' found in log file"

# Dictionary to store basic test stream and sequence of commands that we need to verify
TEST_DATA_1= {
    "title": "TEST ADINSERTION",
    "max_test_time_seconds": 30,
    "expect_list": [
        {"cmd": TESTS[0]['cmd']},
        {"cmd": "progress"},
        {"cmd": DASH_TEST_STREAM},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"position=26\.\d+", "callback": check_skipFetchFragment},
        {"expect": "AAMP_EVENT_EOS"},
    ]
}

# Test case where PTO is greater than the start time, and the delta between the first timeline value and PTO is greater than at least one fragment.

TEST_DATA_2 = {
    "title": "TEST PTO",
    "max_test_time_seconds": 30,
    "expect_list": [
        {"cmd": "seek 812"},
        {"cmd": TESTS[0]['cmd']},
        {"cmd": "progress"},
        {"cmd": DASH_TEST_STREAM_1},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"position=812\.\d+", "callback": check_skipFetchFragment},
    ]
}

TESTLIST = [TEST_DATA_1,TEST_DATA_2]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_1007(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)