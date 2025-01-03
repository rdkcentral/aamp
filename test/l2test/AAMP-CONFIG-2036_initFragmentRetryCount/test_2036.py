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
[{"delay": 10000, "pattern": "(1080|720|480)p_init.m4s"}]
Delays all video init segments downloads from 1080p, 720p by 10 seconds. Only 480p and 360p init segments will be unaffected
"""
simlinearResp=[{"delay": 10000, "pattern": "(1080|720)p_init.m4s"}]

# data = "W3siZGVsYXkiOiAxMCwgInBhdHRlcm4iOiAiKDEwODB8NzIwfDQ4MClwX2luaXQubTRzIn1d"
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')

initFragmentRetryCount = 2
TESTDATA1 = {
    "title": "init Fragment retry count",
    "logfile": f"initFragmentRetryCount_{initFragmentRetryCount}.txt",
    # 3 attempts of 4s each for 2 init fragments. Total 6 attempts are made
    "max_test_time_seconds": 60,
    "aamp_cfg": f"info=true\ntrace=true\ninitFragmentRetryCount={initFragmentRetryCount}\ninitialBitrate=5000000\nnetworkTimeout=4\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        # One original attempt and 2 retry attempts. Total 3 download attempts are actually made
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": r"StreamAbstractionAAMP_MPD: ABR 1920x1080\[5000000\] -> 1280x720\[2800000\]"},
        # Same happens with 720p init fragment as rampdown is in single step
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": r"StreamAbstractionAAMP_MPD: ABR 1280x720\[2800000\] -> 842x474\[1400000\]"},
        {"expect": r"HttpRequestEnd: 2,7,200.*?http://localhost:8085/VideoTestStream/dash/480p_init.m4s"},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        # Wait for some more fragments to be downloaded
        {"expect": r"HttpRequestEnd: 0,0,200.*?http://localhost:8085/VideoTestStream/dash/(480|360)p_010.m4s","end_of_test": True},
    ]
}

initFragmentRetryCount = 3
TESTDATA2 = {
    "title": "init Fragment retry count",
    "logfile": f"initFragmentRetryCount_{initFragmentRetryCount}.txt",
    "max_test_time_seconds": 80,
    "aamp_cfg": f"info=true\ntrace=true\ninitialBitrate=2800000\ninitFragmentRetryCount={initFragmentRetryCount}\nnetworkTimeout=4\n",
    "archive_url": archive_url,
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        # # One original attempt and 3 retry attempts for 720p. Total 4 download attempts are actually made
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:3"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/720p_init.m4s"},
        {"expect": r"StreamAbstractionAAMP_MPD: ABR 1280x720\[2800000\] -> 842x474\[1400000\]"},
        # 480p succeeds
        {"expect": r"HttpRequestEnd: 2,7,200.*?http://localhost:8085/VideoTestStream/dash/480p_init.m4s"},
        {"expect": "AAMP_EVENT_STATE_CHANGED: PLAYING"},
        # Rampup happens once buffer builds up to the top profile
        {"expect": r"StreamAbstractionAAMP_MPD: ABR 842x474\[1400000\] -> 1920x1080\[5000000\]"},
        # One original attempt and 3 retry attempts for 1080p. Total 4 download attempts are actually made
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:1"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:2"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Download failed due to curl timeout or isDownloadStalled:0 Retrying:1 Attempt:3"},
        {"expect": r"(?i)AAMPLogNetworkError error='curl error 28' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        {"expect": "Init fragment fetch failed -- fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_init.m4s"},
        # Wait for some more fragments to be downloaded
        {"expect": r"HttpRequestEnd: 0,0,200.*?http://localhost:8085/VideoTestStream/dash/(480|360)p_010.m4s","end_of_test": True},
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