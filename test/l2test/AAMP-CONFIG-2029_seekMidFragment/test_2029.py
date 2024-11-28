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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/seekMidFragment.tar.xz"

TESTDATA1 = {
    "title": "Testcase to validate seekMidFragment config",
    "logfile": f"seekMidFragment_true.txt",
    "max_test_time_seconds": 20,
    "aamp_cfg": "info=true\ntrace=true\nseekMidFragment=true",
    "archive_url": archive_url,
    "url":"seekMidFragment/test_manifest.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect":"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect":r"Returning Position as 2(\d{3})"},
        {"expect":r"Returning Position as 5(\d{3})"},
        {"cmd": "seek 55"},
        {"expect": r"aamp_Seek\(55\.000000\)"},
        {"expect": r"offsetFromStart\(55\.000000\) seekPosition\(55\.000000\)"},
        {"expect": r"Updated seek_pos_seconds 55\.000000"},
        {"expect": r"TuneHelper - seek_pos: 55\.000000"},
        {"expect": r"Setting PTS offset: 55\.000000 \| 55\.000000"},
        {"expect":r"Returning Position as 56(\d{3})"},
        {"expect":r"Returning Position as 59(\d{3})"},
    ]
}
TESTDATA2 = {
    "title": "Testcase to validate seekMidFragment config",
    "logfile": f"seekMidFragment_false.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nseekMidFragment=false",
    "archive_url": archive_url,
    "url":"seekMidFragment/test_manifest.mpd",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect":"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect":r"Returning Position as 2(\d{3})"},
        {"expect":r"Returning Position as 5(\d{3})"},
        {"cmd": "seek 55"},
        {"expect": r"aamp_Seek\(55\.000000\)"},
        {"expect": r"offsetFromStart\(55\.000000\) seekPosition\(30\.000000\)"},
        {"expect": r"Updated seek_pos_seconds 30\.000000"},
        {"expect": r"TuneHelper - seek_pos: 30\.000000"},
        {"expect": r"Setting PTS offset: 30\.000000 \| 30\.000000"},
        {"expect":r"Returning Position as 33(\d{3})"},
        {"expect":r"Returning Position as 36(\d{3})"},
    ]
}
TESTLIST = [TESTDATA1,TESTDATA2]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2029(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
