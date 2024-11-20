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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/AAMP-IFRAME-4007/VideoTestStream.tar.xz"

def createTestData(fragmentRetryLimit):
    """
    [{"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}]
    Gives HTTP 404 error for all video segments numbered from 006 to 018
    """
    simlinearResp = [
        {"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}
    ]
    data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')

    testdata = {
        "title": "Test case to validate fragmentRetryLimit Configuration",
        "logfile": f"fragmentRetryLimit_{fragmentRetryLimit}.txt",
        "max_test_time_seconds": 30,
        "archive_url": archive_url,
        "url":f"VideoTestStream/main.mpd?respData={data}",
        "simlinear_type": "DASH",
        "aamp_cfg": f"info=true\ntrace=true\nprogress=true\nfragmentRetryLimit={fragmentRetryLimit}",
        "expect_list": [
            {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        ]
    }
    for _ in range(fragmentRetryLimit):
        testdata['expect_list'].append({"expect": r"fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/(1080|720|480|360)p_00[0-9].m4s"})
        testdata['expect_list'].append({"expect": "Condition Rampdown Success"})
    testdata['expect_list'].append({"expect": r"fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/(1080|720|480|360)p_00[0-9].m4s"})
    testdata['expect_list'].append({"expect": f"Rampdown limit reached, Limit is {fragmentRetryLimit}"})
    return testdata

TESTDATA1 = createTestData(fragmentRetryLimit=3)
TESTDATA2 = createTestData(fragmentRetryLimit=2)
TESTDATA3 = createTestData(fragmentRetryLimit=1)

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2037(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
