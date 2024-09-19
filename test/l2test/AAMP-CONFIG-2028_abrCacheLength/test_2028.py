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


from inspect import getsourcefile
import os
import pytest
import re 
PREV_PLAYBACK_POSITION = 0
def check_playback_progress(match):
    global PREV_PLAYBACK_POSITION
    try:
        current_position = int(match.group(1))
    except:
        current_position = 0
    if current_position <= PREV_PLAYBACK_POSITION and PREV_PLAYBACK_POSITION > 0:
        assert 0, "Playback is not progressing" 
    PREV_PLAYBACK_POSITION = current_position 

def generate_testcase(abrCacheLength = 4):
    TESTDATA = {
        "title": "abrCacheLength - Length of abr cache for network bandwidth calculation (# of segments . default 3)",
        "logfile": f"abrCacheLength_{abrCacheLength}.txt",
        "max_test_time_seconds": 30,
        "aamp_cfg": f"info=true\ntrace=true\nabrCacheLength={abrCacheLength}\ninitialBitrate=8000",
        "url":"VideoTestStream/main.mpd",
        "simlinear_type": "DASH",
        "expect_list": [ 
            {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
            {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        ]
    }
    for _ in range(2):
        for _ in range(abrCacheLength+1):
            TESTDATA["expect_list"].append({"expect": r"fragmentUrl http://localhost:8085/VideoTestStream/dash/(1080|720|480|360)p_\d{3}\.m4s"})
        TESTDATA["expect_list"].append({"expect": r"currProfileIndex (\d), newProfileIndex (\d) ,nwBandwidth (\d+) ,bufferValue (\d+).(\d+) ,newBandwidth (\d+)"},)
    TESTDATA["expect_list"].append({"expect": r"Returning Position as (\d+)", "callback": check_playback_progress},)
    TESTDATA["expect_list"].append({"expect": r"Returning Position as (\d+)", "callback": check_playback_progress},)
    return TESTDATA


TESTLIST = [
        generate_testcase(abrCacheLength = 2),
        generate_testcase(abrCacheLength = 6),
        generate_testcase(abrCacheLength = 8)
    ]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2028(aamp_setup_teardown, test_data):
    global PREV_PLAYBACK_POSITION
    PREV_PLAYBACK_POSITION = 0
    
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
