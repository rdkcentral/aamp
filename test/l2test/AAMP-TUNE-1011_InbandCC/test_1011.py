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


DASH_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/InbandCCHarvest/vod2-s8-prd.top.xs.xumo.com/v2/bmff/cenc/t/ipvod20/UPTV0000000008373881/movie/1698194067914/manifest.mpd"
TESTS=[    {"cmd":'setconfig {"info":true,"progress":true,"jsinfo":true}'}
]

# Create instances
playback = {}






@pytest.fixture(params=TESTS)
def test_data(request):
    '''
    This returns a test data of different configurations
    '''
    return request.param

@pytest.mark.ci_test_set
def test_1011(aamp_setup_teardown,test_data):

    #Dictionary to store basic test stream and sequence of commands that we need to verify
    BASIC_TEST_DATA= {
        "title": "Inband CC ",
        "expect_list": [
           {"cmd": DASH_TEST_STREAM},
           {"cmd":"get 22"},
           {"expect": r" AVAILABLE TEXT TRACKS: \[\{" },
           {"expect":r"\"sub-type\":	\"CLOSED-CAPTIONS\""},
           {"expect":r"\"language\":	\"en\""},
           {"expect":r"\"rendition\":	\"urn:scte:dash:cc:cea-608:2015\""},
           {"expect":r"\"instreamId\":	\"CC1\""},
           {"expect":r"\"type\":	\"captions\""},
           {"expect":r"\"availability\":	true"},
           {"expect":r"}, {"},
           {"expect":r"\"sub-type\":	\"CLOSED-CAPTIONS\""},
           {"expect":r"\"language\":	\"en\""},
           {"expect":r"\"rendition\":	\"urn:scte:dash:cc:cea-608:2015\""},
           {"expect":r"\"instreamId\":	\"CC1\""},
           {"expect":r"\"type\":	\"captions\""},
           {"expect":r"\"availability\":	true"},
            {"cmd":"sleep 2000"},
    ]

    }

    # Update playback for each test
    global playback
    playback = copy.deepcopy(BASIC_TEST_DATA)
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    #Passing each test sequence to aampcli through run_expect_a API
    aamp.run_expect_a(playback)


