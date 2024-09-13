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
import sys
import json
import copy
import pytest

from inspect import getsourcefile


DASH_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/DELIA-65821_Multiperiod/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd"

TESTS=[    {"cmd":'setconfig {"info":true,"progress":true,"jsinfo":true}'}
]

# Create instances
playback = {}



#Function to check if Skipping Fetchfragment logline from regular expression match
def check_skipFetchFragment(re_match):
        current_directory = os.getcwd()
        folder_name = 'AAMP-TUNE-1007_AdInsertion/output/'
        file_name = 'test_1007[test_data0].log'
        logfile_path = os.path.join(current_directory, folder_name, file_name) 
        with open(logfile_path, 'r') as file:
            log_content = file.read()
            assert "Skipping Fetchfragment" not in log_content, "Test failed: 'Skipping Fetchfragment' found in log file"





@pytest.fixture(params=TESTS)
def test_data(request):
    '''
    This returns a test data of different configurations
    '''
    return request.param

@pytest.mark.ci_test_set
def test_1007(aamp_setup_teardown,test_data):

    #Dictionary to store basic test stream and sequence of commands that we need to verify
    BASIC_TEST_DATA= {
        "title": "TEST ADINSERTION ",
        "max_test_time_seconds": 30,
        "expect_list": [

            {"cmd": test_data['cmd']},
            {"cmd": "progress"},
            {"cmd": DASH_TEST_STREAM},
            {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
            {"expect": r"position=26\.\d+","callback":check_skipFetchFragment},
            {"expect":"AAMP_EVENT_EOS"},
    ]
    }

    # Update playback for each test
    global playback
    playback = copy.deepcopy(BASIC_TEST_DATA)
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    #Passing each test sequence to aampcli through run_expect_a API
    aamp.run_expect_a(playback)


