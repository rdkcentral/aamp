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
import re
import pytest

from inspect import getsourcefile


sleep_time=5000

def Generate_ExpectList(url, bw):
    
    data = [
        {"cmd": url},
        {"expect": "IP_AAMP_TUNETIME"},
        {"cmd": "sleep {}".format(sleep_time)},
        {"cmd": "get thumbnailConfig"},
        {"expect": "Current ThumbnailTracks: {} .".format(bw)},
        {"cmd": "sleep {}".format(sleep_time)},
        {"cmd": "stop"},
        {"expect": "aamp_stop PlayerState=8"},
        {"cmd": "exit"},
    ]
    return data


stream_configuration=[
    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock1/mpeg_2sec/manifest.m3u8",
        "bw":re.escape(r'[{"RESOLUTION":"416x234","BANDWIDTH":27054},{"RESOLUTION":"336x189","BANDWIDTH":18688},{"RESOLUTION":"224x126","BANDWIDTH":9835}]'),
        "logfile":"tn1"},
    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock2/mpeg_2sec/manifest.m3u8",
        "bw":re.escape(r'[{"RESOLUTION":"416x234","BANDWIDTH":13059},{"RESOLUTION":"336x189","BANDWIDTH":9413},{"RESOLUTION":"224x126","BANDWIDTH":5077}]'),
        "logfile":"tn2"},
]

@pytest.fixture(params = stream_configuration)
def test_data(request):
    return request.param

def test_4002(aamp_setup_teardown, test_data):

    single_test_data = {
        "title": "Test GetAvailableThumbnailTracks API",
        "max_test_time_seconds": 30,
        "aamp_cfg": "info=true\ntrace=true\n",
        "expect_list": Generate_ExpectList(test_data["url"], test_data["bw"])
    }

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))

    aamp.run_expect_a(single_test_data)

