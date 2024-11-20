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
[{"delay": 5, "pattern": "(1080|720|480)p_init.m4s"}]
Gives HTTP 404 error for all video segments numbered from 006 to 018
"""
simlinearResp=[{"delay": 10000, "pattern": "(1080|720|480)p_init.m4s"}]

# data = "W3siZGVsYXkiOiAxMCwgInBhdHRlcm4iOiAiKDEwODB8NzIwfDQ4MClwX2luaXQubTRzIn1d"
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')

initFragmentRetryCount = 2
TESTDATA1 = {
    "title": "init Fragment retry count",
    "logfile": f"initFragmentRetryCount_{initFragmentRetryCount}.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": f"info=true\ntrace=true\ninitFragmentRetryCount={initFragmentRetryCount}\ninitialBitrate=5000000",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Not able to download init fragments; reached failure threshold sending tune failed event"},
        {"expect": "AAMP: init fragment download failed"},
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: init fragment download failed : Curl Error Code 28, Download time expired"},
    ]
}
initFragmentRetryCount = 3
TESTDATA2 = {
    "title": "init Fragment retry count",
    "logfile": f"initFragmentRetryCount_{initFragmentRetryCount}.txt",
    "max_test_time_seconds": 60,
    "aamp_cfg": f"info=true\ntrace=true\ninitialBitrate=2800000\ninitFragmentRetryCount={initFragmentRetryCount}\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:3"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": r"AAMPLogABRInfo : switching to 'higher' profile '1 -> 0' currentBandwidth\[2800000\]->desiredBandwidth\[5000000\] nwBandwidth\[-1\] reason='fragment download failed' error='http error 28' symptom='video quality may increase \(or\) freeze/buffering'"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:3"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Not able to download init fragments; reached failure threshold sending tune failed event"},
        {"expect": "AAMP: init fragment download failed"},
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: init fragment download failed : Curl Error Code 28, Download time expired"},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2036(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)