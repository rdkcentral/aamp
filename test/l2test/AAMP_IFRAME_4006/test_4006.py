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
# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

# Rewind with "rew 8"
TESTDATA1 = {
"title": "Mpd Trick Play Rewind ",
"logfile": "testdata1.txt",
"max_test_time_seconds": 60,
"aamp_cfg": "info=true\ntrace=true\nprogress=true\nprogressReportingInterval=0.25\n",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": r"AAMP_EVENT_TUNED"},
        {"cmd": "seek 300"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
        {"cmd": "get playbackPosition"},
        {"cmd": "rew 8"},

        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-8.000000"},

         # Iframe segments in decreasing order indicates rewind is happening
        {"expect" : r"iframe_28(\d{1})\.m4s"}, 

        {"expect" : r"iframe_2[3-5](\d{1})\.m4s"},
        
        # Expected aamp position [0..187..900..-1..-1.00..0.00....800000..800000..-8.00] here -8.00 at the end confirms playback speed(rewind) is -8
        {"expect": r"aamp pos: \[(.*)" + f"{-8}" + r".00]"},

        {"expect" : r"iframe_1(\d{2})\.m4s"},

    ]
}
# Rewind with "rew 4"
TESTDATA2 = {
"title": "Mpd Trick Play Rewind ",
"logfile": "testdata2.txt",
"max_test_time_seconds": 60,
"aamp_cfg": "info=true\ntrace=true\nprogress=true\nprogressReportingInterval=0.25\n",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        
        {"expect": r"AAMP_EVENT_TUNED"},
        {"cmd": "seek 240"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
        {"cmd": "get playbackPosition"},
        {"cmd": "rew 4"},

        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-4.000000"}, 

         # Iframe segments in decreasing order indicates rewind is happening
        {"expect" : r"iframe_2[3|4](\d{1})\.m4s"},

        {"expect" : r"iframe_2[0-2](\d{1})\.m4s"},

        {"expect" : r"iframe_1(\d{2})\.m4s"},

        # Expected aamp position [0..233..900..-1..-1.00..0.00....800000..800000..-4.00] here -4.00 at the end confirms playback speed(rewind) is -4
        {"expect": r"aamp pos: \[(.*)" + f"{-4}" + r".00]"},

    ]
}

# Rewind with "rew 2"
TESTDATA3 = {
"title": "Mpd Trick Play Rewind ",
"logfile": "testdata3.txt",
"max_test_time_seconds": 60,
"aamp_cfg": "info=true\ntrace=true\nprogress=true\nprogressReportingInterval=0.25\n",
"expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        
        {"expect": r"AAMP_EVENT_TUNED"},
        {"cmd": "seek 180"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
        {"cmd": "get playbackPosition"},
        {"cmd": "rew 2"},

        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

        # Iframe segments in decreasing order indicates rewind is happening
        {"expect" : r"iframe_1[7|8](\d{1})\.m4s"},


        {"expect" : r"iframe_16(\d{1})\.m4s"},

        # Expected aamp position [0..177..900..-1..-1.00..0.00....800000..800000..-2.00] here -2.00 at the end confirms playback speed(rewind) is -2
        {"expect": r"aamp pos: \[(.*)" + f"{-2}" + r".00]"},
    ]
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3]



############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_4006(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


