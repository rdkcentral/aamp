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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events


import os
import sys
import json
import re
import pytest

from inspect import getsourcefile

#Test stream
DASH_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/foxtel-10/single-period.mpd"

test_data=[
{ "url":DASH_TEST_STREAM}
]
@pytest.fixture(params=test_data)
def test_data(request):
    return request.param

def test_13002(aamp_setup_teardown,test_data):
    TESTDATA ={
        "title": "TEST GETAUDIOBITRATES API",
        "max_test_time_seconds": 15,
        "aamp_cfg": "info=true",
        "expect_list": [{"cmd":test_data["url"] },
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"cmd": "get audioBitrates"},
        {"expect":re.escape("AUDIO BITRATES = [ 256000, 128342,  ]")},
        {"cmd": r"stop"},
        {"expect": r"aamp_stop PlayerState"},
        ]}
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.create_aamp_cfg(test_data.get('aamp_cfg'))
    aamp.run_expect_a(TESTDATA)
