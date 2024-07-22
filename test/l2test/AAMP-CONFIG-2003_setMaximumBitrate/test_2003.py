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
import pytest
import re
from inspect import getsourcefile

TESTDATA0 = {
    "title": "Test SetMaximumBitrate() API. Set a valid value",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list": [
	{"cmd": "set maximumBitrate 1000000"},
        {"expect": "set maximumBitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
	{"expect": "Video Profile added to ABR, userData=0 BW=800000"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/360p.m3u8"},
	{"expect": "AAMP_EVENT_TUNED"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":800000"},
	{"cmd": "stop"},
        {"expect": "aamp_stop PlayerState"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Added Video Profile to ABR BW= 800000"},
	{"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/360p_init.m4s"},
	{"expect": "AAMP_EVENT_TUNED"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":800000"},
	{"cmd": "stop"},
        {"expect": "aamp_stop PlayerState"}
    ]
}

TESTDATA1 = {
    "title": "Test SetMaximumBitrate() API. Set an invalid value",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list": [
        {"cmd": "set maximumBitrate -100000"},
        {"expect": "Invalid bitrate value"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/480p.m3u8"},
	{"expect": "AAMP_EVENT_TUNED"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":1400000"},
	{"cmd": "sleep 1"},
	{"cmd": "get videoBitrates"},
	{"expect": re.escape("VIDEO BITRATES = [ 800000, 1400000, 2800000, 5000000,  ]")},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_init.m4s"},
	{"expect": "AAMP_EVENT_TUNED"},
        {"expect": "IP_AAMP_TUNETIME"},
       	{"expect": "\"vfb\":1400000"},
	{"cmd": "sleep 1"},
	{"cmd": "get videoBitrates"},
	{"expect": re.escape("VIDEO BITRATES = [ 5000000, 2800000, 1400000, 800000,  ]")},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState"}
    ]
}

TESTLIST = [TESTDATA0,TESTDATA1]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2003(aamp_setup_teardown,test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
