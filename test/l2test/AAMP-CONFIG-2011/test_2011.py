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

offset = 10
TESTDATA1 = {
    "title": "Test case to validate offset 10",
    "logfile": "aamp_offset_10.txt",
    "max_test_time_seconds": 50,
    # offset is configured as 10 in aamp.cfg
    "aamp_cfg": f"info=true\ntrace=true\noffset={offset}\nprogress=true",
    "expect_list": [
        # Start playback
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        # Playback seeked to 10 seconds and skipped 1st 10 seconds.
        {"expect": re.escape(f"playlistSeek : {offset}.000000 seek_pos_seconds:{offset}.000000")},
        {"expect": re.escape(f"offsetFromStart({offset}.000000)")},
        {"expect": re.escape(f"Setting PTS offset: {offset}.000000")},
        {"expect": re.escape(f"Returning Position as {offset}000 (seek_pos_seconds = {offset}.000000) and updating previous position.")},
        {"expect": re.escape(f"Send first progress event with position {offset}")},
        # Checking playback position at the start of playback, 10 is the position.
        {"cmd": "get playbackPosition"},
        {"expect": re.escape(f"PLAYBACK POSITION = {offset}")},
        # Started Playback from position 10 and progressed at positions 12, 14...
        {"expect": r"Returning Position as 1[1-2](\d{3}) "},
        {"expect": r"Returning Position as 1[3-4](\d{3}) "},
        # Confirming the player config loaded, the offset is set to 10.
        {"cmd": "getconfig"},
        {"expect": f'"offset":{offset}'},
    ]
}

offset = 20
TESTDATA2 = {
    "title": "Test case to validate offset 20",
    "logfile": "aamp_offset_20.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        # offset is configured as 20 after player is started.
        {"cmd":'setconfig {"offset":'+str(offset)+'}'},
        # Start playback
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        # Playback seeked to 20 seconds and skipped 1st 20 seconds.
        {"expect": re.escape(f"playlistSeek : {offset}.000000 seek_pos_seconds:{offset}.000000")},
        {"expect": re.escape(f"offsetFromStart({offset}.000000)")},
        {"expect": re.escape(f"Setting PTS offset: {offset}.000000")},
        {"expect": re.escape(f"Returning Position as {offset}000 (seek_pos_seconds = {offset}.000000) and updating previous position.")},
        {"expect": re.escape(f"Sending segment for mediaType[0]. pts {offset}000000000 dts {offset}000000000")},
        {"expect": re.escape(f"Send first progress event with position {offset}")},
        {"cmd": "get playbackPosition"},
        {"expect": re.escape(f"PLAYBACK POSITION = {offset}")},
        # Started Playback from position 20 and progressed at positions 22, 24...
        {"expect": r"Returning Position as 2[1-3](\d{3}) "},
        {"expect": r"Returning Position as 2[4-7](\d{3}) "},
        # Confirming the player config loaded, the offset is set to 20.
        {"cmd": "getconfig"},
        {"expect": f'"offset":{offset}'},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2]

############################################################
"""
With this fixture we cause the test to be called
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2011(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
