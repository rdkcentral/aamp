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


TESTDATA1 = {
    "title": "StereoOnly Setting as true",
    "logfile": "stereo.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true",
    "expect_list": [
        {"cmd": "set stereoOnlyPlayback 1"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/audiomultichannel/dolby_and_stereo_stream/stream.mpd"},
        {"cmd": "getconfig"},
        {"expect": r'"disableEC3":true'},
        {"expect": r'"disableATMOS":true'},
        {"expect": r'"stereoOnly":true'},
        {"expect": r'"disableAC3":true'},
        {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/public/aamptest/streams/audiomultichannel/dolby_and_stereo_stream/stereo/[1-3]\.m4s"},
        {"cmd": "get audioTrackInfo"},
        {"expect": re.escape(f'"codec":	"mp4a.40.2"')},
        {"expect": r"Returning Position as [1-3](\d{3}) "},
        {"expect": r"Returning Position as [4-6](\d{3}) "},
        # {"expect": r"Returning Position as [7-9](\d{3}) "},

    ]
}
TESTDATA2 = {
    "title": "StereoOnly Setting as false",
    "logfile": "dolby.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true",
    "expect_list": [
        {"cmd": "set stereoOnlyPlayback 0"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/audiomultichannel/dolby_and_stereo_stream/stream.mpd"},
        {"cmd": "getconfig"},
        {"expect": r'"disableEC3":false'},
        {"expect": r'"disableATMOS":false'},
        {"expect": r'"stereoOnly":false'},
        {"expect": r'"disableAC3":false'},
        {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/public/aamptest/streams/audiomultichannel/dolby_and_stereo_stream/dolby/[1-3]\.m4s"},
        {"cmd": "get audioTrackInfo"},
        {"expect": re.escape(f'"codec":	"ec-3"')},
        {"expect": r"Returning Position as [1-3](\d{3}) "},
        {"expect": r"Returning Position as [4-6](\d{3}) "},
        # {"expect": r"Returning Position as [7-9](\d{3}) "},
    ]
}


TESTLIST = [TESTDATA1,TESTDATA2]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2010(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


