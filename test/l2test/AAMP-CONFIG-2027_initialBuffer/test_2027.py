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


TESTDATA0 = { 
    "title" : "Initialbuffer",
    "logfile" : "intialbuffer_5.txt",
    "aamp_cfg" : "trace=true\ninfo=true\nprogress=true\ninitialBuffer=5",
    "max_test_time_seconds" : 20,
    "url" : "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect" : r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_[0-9]{3}.m4s"},
        {"expect" : r"Caching Complete cacheDuration 6 minInitialCacheSeconds 5"},
        {"expect" : r"sending segment at pos:4\.[0-9]{6} dur:2\.[0-9]{6}"},
        {"expect" : r"video - injected cached fragment at pos 4\.[0-9]{6} dur 2\.[0-9]{6}"},
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect" : r"Returning Position as [1-4][0-9]{3}"},
        {"expect" : r"Returning Position as [5-9][0-9]{3}"},
    ]
}


TESTDATA1 = { 
    "title" : "Initialbuffer",
    "logfile" : "intialbuffer_10.txt",
    "aamp_cfg" : "trace=true\ninfo=true\nprogress=true\ninitialBuffer=10",
    "max_test_time_seconds" : 20,
    "url" : "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect" : r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_[0-9]{3}.m4s"},#404 
        {"expect" : r"Caching Complete cacheDuration 10 minInitialCacheSeconds 10"}, #828
        {"expect" : r"sending segment at pos:8+\.[0-9]{6} dur:2+\.[0-9]{6}"}, #831
        {"expect" : r"video - injected cached fragment at pos 8+\.[0-9]{6} dur 2+\.[0-9]{6}"}, #837
        {"expect" : "AAMP_EVENT_STATE_CHANGED: PLAYING"},#861
        {"expect" : r"Returning Position as [1-4][0-9]{3}"},
        {"expect" : r"Returning Position as [5-9][0-9]{3}"},
    ]
}

TESTDATA2 = { 
    "title" : "Initialbuffer",
    "logfile" : "intialbuffer_15.txt",
    "aamp_cfg" : "trace=true\ninfo=true\nprogress=true\ninitialBuffer=15",
    "max_test_time_seconds" : 30,
    "url" : "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect" : r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_[0-9]{3}.m4s"}, #394
        {"expect" : r"Caching Complete cacheDuration 16 minInitialCacheSeconds 15"}, #1011
        {"expect" : r"sending segment at pos:(14+\.[0-9]{6}) dur:(2+\.[0-9]{6})"}, #1016
        {"expect" : r"video - injected cached fragment at pos 14+\.[0-9]{6} dur 2+\.[0-9]{6}"},#1020
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PLAYING"},#1044
        {"expect" : r"Returning Position as [0-9]{1,4}"},
        {"expect" : r"Returning Position as [1-4][0-9]{3}"},
        {"expect" : r"Returning Position as [5-9][0-9]{3}"},
    ]
}


TESTDATA3 = { 
    "title" : "Initialbuffer",
    "logfile" : "intialbuffer_20.txt",
    "aamp_cfg" : "trace=true\ninfo=true\nprogress=true\ninitialBuffer=20",
    "max_test_time_seconds" : 40,
    "url" : "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect" : r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_[0-9]{3}.m4s"},#394
        {"expect" : r"Caching Complete cacheDuration 20 minInitialCacheSeconds 20"},#1133
        {"expect" : r"sending segment at pos:18+\.0{6} dur:2+\.0{6}"}, #1136
        {"expect" : r"video - injected cached fragment at pos 18{1,2}\.[0-9]{6} dur 2+\.[0-9]{6}"}, #1142
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PLAYING"},#1044
        {"expect" : r"Returning Position as [1-4][0-9]{3}"},
        {"expect" : r"Returning Position as [5-9][0-9]{3}"},
    ]
}

TESTLIST = [TESTDATA0,TESTDATA1,TESTDATA2,TESTDATA3]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
    
def test_2027(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    # print(test_data['title'])
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
