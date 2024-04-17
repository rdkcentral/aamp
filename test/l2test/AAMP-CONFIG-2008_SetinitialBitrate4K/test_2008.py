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

bitrate_1 = 2859078
TESTDATA1 = {
    "title": "Test case to validate InitialBitrate4K",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate4k {bitrate_1}"},
        {"expect": " Matched Command InitialBitrate4k"},
        {"cmd": "https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=2859078"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=1280x720@23.976024fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate4k"},
        {"expect": f" INITIAL BITRATE 4K = {bitrate_1}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate4K":' + str(bitrate_1)},
        {"cmd": "stop"}
    ]
}
# checking bitrate value as 4872811

bitrate_2 = 4872811
TESTDATA2 = {
    "title": "Test case to validate InitialBitrate4K",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate4k {bitrate_2}"},
        {"expect": " Matched Command InitialBitrate4k"},
        {"cmd": "https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=4872811"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=1920x1080@23.976024fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate4k"},
        {"expect": f" INITIAL BITRATE 4K = {bitrate_2}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate4K":' + str(bitrate_2)},
        {"cmd": "stop"}
    ]
}
# checking bitrate value as 7322004

bitrate_3 = 7322004
TESTDATA3 = {
    "title": "Test case to validate InitialBitrate4K",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate4k {bitrate_3}"},
        {"expect": " Matched Command InitialBitrate4k"},
        {"cmd": "https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=7322004"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=2880x1620@23.976024fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Returning Position as 1(\d{3}) "},
        {"cmd": "sleep 5000"},
        {"expect": r"Returning Position as 2(\d{3}) "},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate4k"},
        {"expect": f" INITIAL BITRATE 4K = {bitrate_3}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate4K":' + str(bitrate_3)},
        {"cmd": "stop"}
    ]
}

bitrate_4 = 9604428
TESTDATA4 = {
    "title": "Test case to validate InitialBitrate4K",
    "max_test_time_seconds": 25,
    "aamp_cfg": "info=true\ntrace=true\n",
    "expect_list": [
        {"cmd": f"set initialBitrate4k {bitrate_4}"},
        {"expect": " Matched Command InitialBitrate4k"},
        {"cmd": "https://dash.akamaized.net/akamai/streamroot/050714/Spring_4Ktest.mpd"},
        {"expect": "AAMP_EVENT_BITRATE_CHANGED"},
        {"expect": "bitrate=9604428"},
        {"expect": "description=\"BitrateChanged - Reset to default bitrate due to tune\""},
        {"expect": "resolution=3840x2160@23.976024fps"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"cmd": "sleep 10000"},
        {"expect": r"sleep complete"},
        {"cmd":"get initialBitrate4k"},
        {"expect": f" INITIAL BITRATE 4K = {bitrate_4}"},
        {"cmd":"getconfig"},
        {"expect":'"initialBitrate4K":' + str(bitrate_4)},
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

def test_2008(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
