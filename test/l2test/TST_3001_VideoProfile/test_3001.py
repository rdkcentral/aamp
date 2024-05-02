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

from inspect import getsourcefile
import pytest

TESTDATA1 = {
    "max_test_time_seconds": 90,
    "title": "TESTDATA1",
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list":
    [
        {"cmd": "bps 800000"},
        {"expect": "Set video bitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/360p.m3u8"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t800000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"},
        {"cmd": "sleep 1000"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/360p_init.m4s"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t800000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"}
    ]
}
TESTDATA2 = {
    "max_test_time_seconds": 90,
    "title": "TESTDATA2",
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list":
    [
        {"cmd": "bps 1400000"},
        {"expect": "Set video bitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/480p.m3u8"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t1400000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"},
        {"cmd": "sleep 1000"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_init.m4s"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t1400000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"}
    ]
}
TESTDATA3 = {
    "max_test_time_seconds": 90,
    "title": "TESTDATA3",
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list":
    [
        {"cmd": "bps 2800000"},
        {"expect": "Set video bitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/720p.m3u8"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t2800000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"},
        {"cmd": "sleep 1000"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t2800000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"}
    ]
}
TESTDATA4 = {
    "max_test_time_seconds": 90,
    "title": "TESTDATA4",
    "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
    "expect_list":
        [
        {"cmd": "bps 5000000"},
        {"expect": "Set video bitrate"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": "Inserted. url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/1080p.m3u8"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t5000000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"},
        {"cmd": "sleep 1000"},
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": "Inserted init url https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "IP_AAMP_TUNETIME"},
        {"expect": "\"vfb\":\\t5000000"},
        {"cmd": "sleep 30000"},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"}
    ]
}

TESTDATA = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]

@pytest.fixture(params=TESTDATA)
def test_data(request):
    return request.param

def test_3001(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


