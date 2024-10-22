# !/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
# Copyright 2024 RDK Management
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
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
    "title":"Forcehttp",
    "aamp_cfg":"trace=true\ninfo=true\nprogress=true\nforceHttp=true",
    "max_test_time_seconds":60,
    "url":"https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd",
    "expect_list":[
         {"expect":r"mManifestUrl: http://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd"},
         {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING "},
         {"expect":r"aamp url:3,4,0,0.000000,http://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd"},
         {"expect":r"fragmentUrl http://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/ED_MPEG2_32k_init.mp4"},
         {"expect":r"fragmentUrl http://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/ED_MPEG2_32k_0.mp4"},
         {"expect":r"Returning Position as [0-4](\d{3})"},
         {"expect":r"Returning Position as [5-9](\d{3})"},
    ]
}
TESTDATA1 = {
    "title":"Forcehttp",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\nforceHttp=false",
    "max_test_time_seconds":60,
    "url":"https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd",
    "expect_list":[
         {"expect":r"mManifestUrl: https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd"},
         {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING "},
         {"expect":r"aamp url:3,4,0,0.000000,https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/MultiResMPEG2.mpd"},
         {"expect":r"fragmentUrl https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/ED_MPEG2_32k_init.mp4"},
         {"expect":r"fragmentUrl https://dash.akamaized.net/dash264/TestCases/2c/qualcomm/1/ED_MPEG2_32k_0.mp4"},
         {"expect":r"Returning Position as [0-4](\d{3})"},
         {"expect":r"Returning Position as [5-9](\d{3})"},
    ]
}

TESTLIST = [TESTDATA0,TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
    
def test_2038(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
