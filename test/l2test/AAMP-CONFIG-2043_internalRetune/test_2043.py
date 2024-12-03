#  !/usr/bin/env python3
#  If not stated otherwise in this file or this component's LICENSE file the
#  following copyright and licenses apply:
#  Copyright 2024 RDK Management
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  http://www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from inspect import getsourcefile
import os
import pytest
import re 

# Define test data configurations
TESTDATA0 = { 
    "title": "internalRetune_true",
    "logfile": "internalRetune_1.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\n",
    "max_test_time_seconds": 60,
    "url": "discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        {"cmd": 'setconfig {"internalRetune": true}'},
        {"cmd": "http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd"},
        {"expect": r"mManifestUrl: http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect": r"fragmentUrl http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/../../Content/video/h265/2000k/tears_of_steel_1080p_2000k_h265_dash_track1_init.mp4"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"cmd": "seek 50"},
        {"expect": r"Returning Position as [0-6](\d{4})"},
        {"expect": r"fragmentUrl http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/../../Content/video/h265/2000k/tears_of_steel_1080p_2000k_h265_dash_track1_(\d+)\.m4s"},
        {"expect": r"PrivateInstanceAAMP: Schedule Retune errorType 6 error GstPipeline Internal Error"},
        {"expect": r"Downloads not blocked, track download status: video not blocked, audio not blocked, text blocked, aux_audio blocked"},
        {"expect": r"Found entry in function queue!!, task:PrivateInstanceAAMP_Retune. State:8: CurrentTaskId:4"},
        {"expect": r"Execution:PrivateInstanceAAMP_Retune taskId:4"},
        {"expect": r"playlistSeek : 60.000000 seek_pos_seconds:60.000000 culledSeconds : 0.000000"},
        {"expect": r"Returning Position as [6-7](\d{4})"},
        {"expect": r"Returning Position as [8-9](\d{4})"},
    ]
}

TESTDATA1 = { 
    "title": "internalRetune_false",
    "logfile": "internalRetune_2.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\ninternalRetune=false",
    "max_test_time_seconds": 90,
    "url": "discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd",
    "simlinear_type": "DASH",
    "cmdlist": [
        "seek 58"
    ],
    "expect_list": [
        {"expect": r"mManifestUrl: http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd","min": 0, "max": 3},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING","min": 0, "max": 3},
        {"expect": r"fragmentUrl http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/../../Content/video/h265/2000k/tears_of_steel_1080p_2000k_h265_dash_track1_init.mp4","min": 0, "max": 3},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING","min": 0, "max": 3},
        {"expect": r"http://localhost:8085/discontinuityTestStream/dash\.akamaized\.net/dash264/TestCasesIOP33/Content/video/h265/2000k/tears_of_steel_1080p_2000k_h265_dash_track1_(\d+)\.m4s","min": 0, "max": 3},
        {"expect": r"PrivateInstanceAAMP: Ignore reTune as disabled in configuration","min": 0, "max": 10},
        {"expect": r"Execution:PrivateInstanceAAMP_Retune taskId:4","min": 0, "max": 10, "not_expected" : True},
        {"expect": r"playlistSeek : 60.000000 seek_pos_seconds:60.000000 culledSeconds : 0.000000","min": 0, "max": 10, "not_expected" : True},
        {"expect": r"Returning Position as [6-7](\d{4})","min": 11, "max": 15, "end_of_test": True},
  
    ]
}
############################################################
def test_2043_0(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA0)

def test_2043_1(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA1)
