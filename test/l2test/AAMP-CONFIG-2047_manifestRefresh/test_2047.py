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
import base64
import json

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/sky/skywitness-4klive-8M.tar.gz"
TESTDATA1 = {
    "title": "Test case to validate manifestRefreshEvent",
    "logfile": "ManifestRefreshLogs.txt",
    "max_test_time_seconds": 12,
    "archive_url": archive_url,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\njsinfo=true\nprogress=true",
    "expect_list": [
       {"expect": r"\[AAMPCLI\] AAMP_EVENT_MANIFEST_REFRESH_NOTIFY.*manifestType\[dynamic\]"}
    ],
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

def test_2047(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
