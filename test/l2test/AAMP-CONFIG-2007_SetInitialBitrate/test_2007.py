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

bitrate_1 = 5000000
TESTDATA1 = {
    "title": "Test case to validate InitialBitrate",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate {bitrate_1}"},
        {"expect": " Matched Command InitialBitrate"},
        

        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        
        {"expect": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/1080p_001.m4s"},

        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=5000000"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=1920x1080@25.000000fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED:\ PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        
        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_1}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate":' + str(bitrate_1)},
        {"cmd": "stop"}
    ]
}

bitrate_2 = 2800000
TESTDATA2 = {
    "title": "Test case to validate InitialBitrate",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate {bitrate_2}"},
        {"expect": " Matched Command InitialBitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/720p_001.m4s"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=2800000"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=1280x720@25.000000fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED:\ PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_2}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate":' + str(bitrate_2)},
        {"cmd": "stop"}
    ]
}

bitrate_3 = 1400000
TESTDATA3 = {
    "title": "Test case to validate InitialBitrate",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate {bitrate_3}"},
        {"expect": " Matched Command InitialBitrate"},
        
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_001.m4s"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=1400000"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=842x474@25.000000fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED:\ PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_3}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate":' + str(bitrate_3)},
        {"cmd": "stop"}
    ]
}
bitrate_4 = 800000
TESTDATA4 = {
    "title": "Test case to validate InitialBitrate",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate {bitrate_4}"},
        {"expect": " Matched Command InitialBitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/360p_001.m4s"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=800000"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=640x360@25.000000fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED:\ PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate"},
        {"expect": f"INITIAL BITRATE = {bitrate_4}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate":' + str(bitrate_4)},
        {"cmd": "stop"}
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]
############################################################
"""
With this fixture we cause the test to be called
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
def test_2007(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
