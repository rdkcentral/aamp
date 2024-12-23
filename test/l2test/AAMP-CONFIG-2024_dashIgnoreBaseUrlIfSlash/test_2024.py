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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/TestDash.tar.xz"

TESTDATA1 = {
    "title": "dashIgnoreBaseUrlIfSlash",
    "logfile": f"dashIgnoreBaseUrlIfSlash_1.txt",
    "max_test_time_seconds": 80,
    "aamp_cfg": "info=true\ntrace=true\n",
    "archive_url": archive_url,
    "simlinear_type": "DASH",
    "expect_list": [
        {"cmd": 'setconfig {"dashIgnoreBaseUrlIfSlash":true}'},
        {"expect":"Parsed value for property dashIgnoreBaseUrlIfSlash - true"},
        {"cmd": "http://localhost:8085/TestDashIgnore/demo_manifest.mpd"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":"AAMP_EVENT_PLAYLIST_INDEXED"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: PREPARING"},
        {"expect":r"ignoring baseurl /"},
        {"expect":r"fragmentUrl http://localhost:8085/TestDashIgnore/video_init.mp4"},
        {"expect":r"init url http://localhost:8085/TestDashIgnore/video_init.mp4"},
        {"expect":r"Returning Position as 2(\d{3})"},
        {"expect":r"Returning Position as 5(\d{3})"},
    ]
}
TESTDATA2 = {
    "title": "dashIgnoreBaseUrlIfSlash",
    "logfile": f"dashIgnoreBaseUrlIfSlash_2.txt",
    "max_test_time_seconds": 80,
    "aamp_cfg": "info=true\ntrace=true\n",
    "archive_url": archive_url,
    "simlinear_type": "DASH",
    "expect_list": [
        {"cmd": 'setconfig {"dashIgnoreBaseUrlIfSlash":false}'},
        {"expect":r"Parsed value for property dashIgnoreBaseUrlIfSlash - false"},
        {"cmd": "http://localhost:8085/TestDashIgnore/demo_manifest.mpd"},
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect":"AAMP_EVENT_PLAYLIST_INDEXED"},
        {"expect":"AAMP_EVENT_STATE_CHANGED: PREPARING"},
        {"expect":"Init fragment fetch failed -- fragmentUrl http://localhost:8085/video_init.mp4"},
        {"expect":"AAMP_EVENT_TUNE_FAILED reason=AAMP: init fragment download failed : Http Error Code 404"},
    ]
}
TESTLIST = [TESTDATA1,TESTDATA2]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2024(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
