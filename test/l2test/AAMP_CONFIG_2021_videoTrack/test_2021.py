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

TESTDATA1 = {
"title": "setting videobitare 4k video",
"max_test_time_seconds": 20,
"aamp_cfg": "info=true\nprogress=true\n",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/RDKAAMP-1540/4k/h265/SegmentTimeline.mpd"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = 8000000"},
        {"cmd": "sleep 2000"},
        {"expect": "sleep complete"},
        {"cmd": "set videoTrack 800000 2800000 5000000"},
        {"expect":r"User Profile filtering bitrate size:3 status:1"},
        {"expect":r"User Profile Index : 0\(3\) Bw : 800000"},
        {"expect":r"User Profile Index : 1\(3\) Bw : 2800000"},
        {"expect":r"User Profile Index : 2\(3\) Bw : 5000000"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = (800000|5000000|2800000)"},
        {"cmd": "sleep 2000"},
        {"expect": "sleep complete"},
        {"cmd": "set videoTrack 800000 800000 800000"},
        {"expect":r"User Profile filtering bitrate size:3 status:1"},
        {"expect":r"User Profile Index : 0\(3\) Bw : 800000"},
        {"expect":r"User Profile Index : 1\(3\) Bw : 800000"},
        {"expect":r"User Profile Index : 2\(3\) Bw : 800000"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = 800000"},
        {"cmd": "sleep 2000"},
        {"expect": "sleep complete"},
        {"cmd":"get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = 800000"},
    ]
}

TESTDATA2 = {
"title": "setting videobitare not4k video",
"max_test_time_seconds": 20,
"aamp_cfg": "info=true\nprogress=true\ninitialBitrate=800000",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set videoTrack 2800000 1400000 5000000"},
        {"expect":r"User Profile filtering bitrate size:3 status:1"},
        {"expect":r"User Profile Index : 0\(3\) Bw : 2800000"},
        {"expect":r"User Profile Index : 1\(3\) Bw : 1400000"},
        {"expect":r"User Profile Index : 2\(3\) Bw : 5000000"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = (2800000|1400000|5000000)"},
        {"cmd": "sleep 2000"},
        {"expect": "sleep complete"},
        {"cmd": "set videoTrack 800000 800000 800000"},
        {"expect":r"User Profile filtering bitrate size:3 status:1"},
        {"expect":r"User Profile Index : 0\(3\) Bw : 800000"},
        {"expect":r"User Profile Index : 1\(3\) Bw : 800000"},
        {"expect":r"User Profile Index : 2\(3\) Bw : 800000"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = 800000"},

]
}
TESTDATA3 = {
"title": "setting videobitare not4k video",
"max_test_time_seconds": 20,
"aamp_cfg": "info=true\nprogress=true\ninitialBitrate=800000",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set videoTrack 5000000 5000000 5000000"},
        {"expect":r"User Profile filtering bitrate size:3 status:1"},
        {"expect":r"User Profile Index : 0\(3\) Bw : 5000000"},
        {"expect":r"User Profile Index : 1\(3\) Bw : 5000000"},
        {"expect":r"User Profile Index : 2\(3\) Bw : 5000000"},
        {"cmd": "sleep 2000"},
        {"expect": "sleep complete"},
        {"cmd": "get videoBitrate"},
        {"expect":r"CURRENT VIDEO PROFILE BITRATE = 5000000"},
]
}
TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2021(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)



