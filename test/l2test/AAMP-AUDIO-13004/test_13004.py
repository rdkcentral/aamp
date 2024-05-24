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
import pytest

from inspect import getsourcefile


TEST_DATA_PART = [

    [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 2"},
        {"expect": "Matched Command AudioTrack - set 42 2"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"eng\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ],
    [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 0"},
        {"expect": "Matched Command AudioTrack - set 42 0"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"ger\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ],
    [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 4"},
        {"expect": "Matched Command AudioTrack - set 42 4"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"spa\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ],
    [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 6"},
        {"expect": "Matched Command AudioTrack - set 42 6"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"fra\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ],
    [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 8"},
        {"expect": "Matched Command AudioTrack - set 42 8"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"pol\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ]
]


@pytest.fixture(params=TEST_DATA_PART)
def test_data(request):
    return request.param

# Get all audio tracks : get 20
# Set audio track : set 42 <track_id> 


def test_13004(aamp_setup_teardown,test_data):

    full_test_data = {
        "title": "Test multi audio profile",
        "max_test_time_seconds": 90,
        "aamp_cfg": "info=true\ntrace=true\nabr=false\n",
        "expect_list": test_data
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(full_test_data)



