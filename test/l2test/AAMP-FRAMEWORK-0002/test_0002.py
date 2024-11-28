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
"""
[{"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}]
Gives HTTP 404 error for all video segments numbered from 006 to 018
"""
simlinearResp = [
        {"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')
print(f"{type(data)} | {data = }")
TESTDATA1 = {
    "title": "Testcase to test simlinear's modified response of HTTP 404",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\ninitialBitrate=5000000\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": r"(?i)AAMPLogNetworkError error='http error 404' type='VIDEO' location='unknown' symptom='freeze/buffering' url='http://localhost:8085/VideoTestStream/dash/1080p_006\.m4s\?respData="+data+"'"},
        {"expect": r"fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/(1080|720|480)p_006\.m4s\?respData="+data},
        {"expect": r"(?i)AAMPLogNetworkError error='http error 404' type='VIDEO' location='unknown' symptom='freeze/buffering' url='http://localhost:8085/VideoTestStream/dash/720p_006\.m4s\?respData="+data+"'"},
        {"expect": r"fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/(1080|720|480)p_006\.m4s\?respData="+data},
    ]
}

"""
[{"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"},{"delay": 10, "pattern": "1080p_init.m4s"}]
Gives HTTP 404 error for all video segments numbered from 006 to 018 and add 10 seconds of delay while responding 1080p_init.m4s segment
"""
simlinearResp = [
        {"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"},
        {"delay": 10000, "pattern": "1080p_init.m4s"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')
TESTDATA2 = {
    "title": "Testcase to test simlinear's delay in response",
    "logfile": "testdata2.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\ninitialBitrate=5000000\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='init_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init\.m4s\?respData="+data+"'"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='init_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init\.m4s\?respData="+data+"'"},
    ]
}

"""
Test playback of stream without respData parameter
"""
TESTDATA3 = {
    "title": "Testcase to test simlinear original behaviour",
    "logfile": "testdata3.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\ninitialBitrate=5000000\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": r"Inserted init url http://localhost:8085/VideoTestStream/dash/1080p_init\.m4s"},
        {"expect": r"HttpRequestEnd: ([0-9,.]+)http://localhost:8085/VideoTestStream/dash/1080p_001\.m4s"},
        {"expect": r"HttpRequestEnd: ([0-9,.]+)http://localhost:8085/VideoTestStream/dash/1080p_002\.m4s"},
        {"expect": r"Returning Position as [0-3][0-9]{3}"},
        {"expect": r"Returning Position as [4-8][0-9]{3}"},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_0002(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


