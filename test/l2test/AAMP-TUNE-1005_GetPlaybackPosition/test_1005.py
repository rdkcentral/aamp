#!/usr/bin/env python3

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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events


import os
import sys
import json
import copy
import pytest

from inspect import getsourcefile

#Test stream
DASH_TEST_STREAM = "https://livesim.dashif.org/livesim/segtimeline_1/testpic_2s/Manifest.mpd"


TESTS=[
    {"cmd":'setconfig {"info":true,"progress":true,"enableSeekableRange": true}'},
    {"cmd":'setconfig {"info":true,"progress":true,"enableSeekableRange": true, "useAbsoluteTimeline": true}'},
    {"cmd":'setconfig {"info":true,"progress":true,"enableSeekableRange": true, "useAbsoluteTimeline": true, "preferredAbsoluteReporting": 0}'},
    {"cmd":'setconfig {"info":true,"progress":true,"enableSeekableRange": true, "useAbsoluteTimeline": true, "preferredAbsoluteReporting": 1}'}
]

# Create instances
playback = {}

#The APIs are employed to create a test sequence,
#which must be executed with a specified configuration
#on the given test stream.

#Function to read position from regular expression match
def read_position(re_match):
    if re_match.group(1):
        currentPosition = float(re_match.group(1)) * 1000
    #Dynamically add a test expect string to match the live position. This value is taken on run time
    playback["expect_list"].insert(-2, {"expect": "position={}".format(currentPosition)})


@pytest.fixture(params=TESTS)
def test_data(request):
    '''
    This returns a test data of different configurations
    '''
    return request.param

@pytest.mark.ci_test_set
def test_1005(aamp_setup_teardown,test_data):
    '''
    This test set perform tests on different configurations
    on getPlaybackPosition API.
    '''
    #Dictionary to store basic test stream and sequence of commands that we need to verify
    BASIC_TEST_DATA = {
        "title": "TEST GETPLAYBACKPOSITION API",
        "max_test_time_seconds": 20,
        "expect_list": [
            {"cmd": test_data['cmd']},
            {"cmd": "progress"},
            {"cmd": DASH_TEST_STREAM},
            {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
            {"cmd": "pause"},
            {"expect": "rate=0.000000"},
            {"cmd": "get playbackPosition"},
            {"expect": r"PLAYBACK POSITION = (\d+\.\d+)", "callback": read_position},
            {"expect": "AAMP_EVENT_PROGRESS"},
            {"cmd": "sleep 2000"},
            {"cmd": "stop"},
    ]
    }
    # Update playback for each test
    global playback
    playback = copy.deepcopy(BASIC_TEST_DATA)
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    #Passing each test sequence to aampcli through run_expect_a API
    aamp.run_expect_a(playback)


