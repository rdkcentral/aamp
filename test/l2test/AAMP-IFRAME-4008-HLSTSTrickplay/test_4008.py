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
    "title": "HLS TS IFrame trickmodes",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        # Start Playback
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        
        # Start playback
        {"cmd": "play"}, 
        {"expect": r"\[sendSegment\]\[\d+\]updatedPosition = 0.000000 Position = 0.000000 m_startPosition = 0.000000 m_playRate = 1.000000"},

        # Sleep for 3 seconds
        {"cmd": "sleep 3000"},
        {"expect": "sleeping for 3.000000 seconds"},

        # fast forward 
        {"cmd": "ff4"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate\ \(4\.000000\)"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate rate\(1\.000000\)->\(4\.000000\)"},
        {"expect": r"\[sendSegment\]\[\d+\]updatedPosition = 0.000000 Position = [1-6].\d{6} m_startPosition = [1-6].\d{6} m_playRate = 4.000000"},

        # Sleep for 5 seconds
        {"cmd": "sleep 5000"},
        {"expect": "sleeping for 5.000000 seconds"},

        # Resume playback
        {"cmd": "play"}, 
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
        {"expect": "updatedPosition = 0.000000 Position = 0.000000 m_startPosition = 0.000000 m_playRate = 1.000000"},

        # fast forward 
        {"cmd": "ff30"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate\ \(30\.000000\)"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate rate\(1\.000000\)->\(30\.000000\)"},
        {"expect": r"\[sendSegment\]\[\d+\]updatedPosition = 0.000000 Position = [1-3][0-9].\d{6} m_startPosition = [1-3][0-9].\d{6} m_playRate = 30.000000"},

        # Sleep for 3 seconds
        {"cmd": "sleep 3000"},
        {"expect": "sleeping for 3.000000 seconds"},

        # Resume playback
        {"cmd": "play"}, 
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate\ \(1\.000000\)"},
        {"expect": r"\[SetRateInternal\]\[\d+\]aamp_SetRate rate\(30\.000000\)->\(1\.000000\)"},

        # Sleep for 2 seconds
        {"cmd": "sleep 2000"},
        {"expect": "sleeping for 2.000000 seconds"},

        # Stop playback
        {"cmd": "stop"}, 
    ]
}

TESTLIST = [TESTDATA1]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_4008(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)

