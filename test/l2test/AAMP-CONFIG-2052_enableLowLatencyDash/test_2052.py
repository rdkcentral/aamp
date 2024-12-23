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
TESTDATA1 = { 
    "title": "enableLowLatencyDash_true",
    "logfile": "enableLowLatencyDash_1.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\n",
    "max_test_time_seconds": 30,
    "expect_list": [
        {"cmd": 'setconfig {"enableLowLatencyDash": true}'},
        {"cmd":"https://akamaibroadcasteruseast.akamaized.net/cmaf/live/657078/akasource/out.mpd"},
        {"expect":r"mManifestUrl: https://akamaibroadcasteruseast.akamaized.net/cmaf/live/657078/akasource/out.mpd"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":r"Min Update Period from Manifest 500000 Latency Value -1 lowLatencyMode 1"},
        {"expect":r"StreamAbstractionAAMP_MPD: LL-DASH playback enabled availabilityTimeOffset=1.969000,fragmentDuration=2.002000"},
        {"expect":r"StreamAbstractionAAMP_MPD:\[LL\-Dash\] Min Latency: 3 Max Latency: 9 Target Latency: 6"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Returning Position as [0-6](\d{4})"},
       
    ]
}

TESTDATA2 = { 
    "title": "enableLowLatencyDash_false",
    "logfile": "enableLowLatencyDash_2.txt",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\n",
    "max_test_time_seconds": 60,
    "expect_list": [
        {"cmd": 'setconfig {"enableLowLatencyDash": false}'},
        {"cmd":"https://akamaibroadcasteruseast.akamaized.net/cmaf/live/657078/akasource/out.mpd"},
        {"expect":r"mManifestUrl: https://akamaibroadcasteruseast.akamaized.net/cmaf/live/657078/akasource/out.mpd"},
        {"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":r"Min Update Period from Manifest 500000 Latency Value -1 lowLatencyMode"},
        {"expect":r"StreamAbstractionAAMP_MPD: LL-DASH playback disabled in config"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Returning Position as [0-6](\d{4})"},
        
     
    ]
}
############################################################
TESTLIST = [TESTDATA1,TESTDATA2]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2052(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
