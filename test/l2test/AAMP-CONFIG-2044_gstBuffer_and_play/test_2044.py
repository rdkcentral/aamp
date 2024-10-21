# !/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
# 
# Copyright 2024 RDK Management
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
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
    "title":"gstBufferAndPlay",
    "aamp_cfg":"trace=true\ninfo=true\nprogress=true\ngstBufferAndPlay=true",
    "max_test_time_seconds":60,
    "url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect":r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/en_init.m4s"},
        {"expect":r"AAMPGstPlayerPipeline buffering_enabled 1"},
        {"expect":r"Set pipeline state to PLAYING - buffering_timeout_cnt 101  frames -1"},
        {"expect":r"AAMPGstPlayer: AAMPGstPlayerPipeline state set to PLAYING, rc:1"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect":r"fragmentUrl https:\/\/cpetestutility\.stb\.r53\.xcal\.tv\/VideoTestStream\/dash\/(\d+p)_(\d+)\.m4s"},
        {"expect":r"Returning Position as [0-4](\d{3})"},
        {"expect":r"Returning Position as [5-9](\d{3})"},
    ]
}
TESTDATA1 = { 
    "title":"gstBufferAndPlay",
    "aamp_cfg":"trace=true\ninfo=true\nprogress=true\ngstBufferAndPlay=false",
    "max_test_time_seconds":60,
    "url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
    "expect_list" :[
        {"expect":r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/en_init.m4s"},
        {"expect":r"AAMPGstPlayerPipeline buffering_enabled 0"},
        {"expect":r"AAMPGstPlayer: AAMPGstPlayerPipeline state set to PLAYING, rc:2"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect":r"fragmentUrl https:\/\/cpetestutility\.stb\.r53\.xcal\.tv\/VideoTestStream\/dash\/(\d+p)_(\d+)\.m4s"},
        {"expect":r"Returning Position as [0-4](\d{3})"},
        {"expect":r"Returning Position as [5-9](\d{3})"}
    ]
}
TESTLIST = [TESTDATA0,TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
    
def test_2044(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
