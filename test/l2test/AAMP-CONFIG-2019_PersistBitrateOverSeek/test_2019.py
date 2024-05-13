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

bitrate_1 = 5000000
TESTDATA1 = {
    "title": "Test case to validate PersistBitrateOverSeek",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 300,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\nprogressReportingInterval=0.25\n",
    "expect_list": [
        {"cmd":'setconfig {"persistBitrateOverSeek":true}'},
        {"expect": f"persistBitrateOverSeek New Owner"},
        {"cmd": f"set initialBitrate {bitrate_1}"},
        {"expect": " Matched Command InitialBitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        
        {"cmd": "seek 180"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},

        #Media_segment
        {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/1080p_0[0-4](\d){1}\.m4s"},

        {"expect": r"Returning Position as 1(\d{5}) "},

        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_1}"},
        {"cmd": "sleep 7000"},
        {"expect": r"sleep complete"},

        {"cmd":"ff 2"},
        {"expect": r"Returning Position as 1(\d{5}) "},

        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=2.000000"},

        {"expect" : r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_1[8|9](\d{1})\.m4s"},

        {"expect" : r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/iframe_20(\d{1})\.m4s"},

        {"cmd": "sleep 7000"},
        
        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_1}"},

    ]
}

TESTLIST = [TESTDATA1]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2011(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)



