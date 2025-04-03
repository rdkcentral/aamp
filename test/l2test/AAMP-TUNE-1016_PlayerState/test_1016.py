#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

# Starts aamp-cli and initiates playback by giving it a DASH stream URL
# Stops stream after ~1s of playback, and ensure that player state flows through STOPPING, with RELEASED state reached after all resources actually released
# verifies aamp log output against expected list of events
# Also see README.md

from inspect import getsourcefile
import os
import pytest

    
TESTDATA0 = {
"title": "DASH",
"max_test_time_seconds": 10,
"aamp_cfg": "info=true\n",
"expect_list": [
    {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
    {"expect": "AAMP_EVENT_TUNED"},
    {"cmd":"sleep 1000"},
    {"cmd": "stop"},
    { "expect": "aamp_stop PlayerState=8"},
    { "expect": "AAMP_EVENT_STATE_CHANGED: STOPPING"},
    { "expect": "Calling delete of Downloader instance"},
    { "expect": "AAMP_EVENT_STATE_CHANGED: RELEASED"}
]
}


TESTLIST = [TESTDATA0]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

@pytest.mark.ci_test_set
def test_1015(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
