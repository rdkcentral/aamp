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



TESTDATA = {
    "title": "Test case to validate monitorAV,monitorAVSyncThreshold,monitorAVJumpThreshold",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 10,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
       	{"cmd": 'setconfig {"monitorAV": true}'},
        {"cmd": 'setconfig {"monitorAVSyncThreshold": 1000}'},
        {"cmd": 'setconfig {"monitorAVJumpThreshold": 1000}'},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
      	{"expect": f"monitorAV New Owner"},
        {"expect": r"Parsed value for property monitorAV - true"},
        {"expect": f"monitorAVSyncThreshold New Owner"},
        {"expect": r"Parsed value for property monitorAVSyncThreshold - 1000"},
        {"expect": f"monitorAVJumpThreshold New Owner"},
        {"expect": r"Parsed value for property monitorAVJumpThreshold - 1000"},
    ]
}
############################################################
def test_2074(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA)

