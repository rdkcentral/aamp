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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/VideoTestStream_HLS.tar.xz"

"""
[{"status": 404, "pattern": "(1080|720|480)p.m3u8"}]
Setting initial bitrate to highest available bitrate (5000000) to start 1080p profile playback
Returning Http 404 error for 1080p, 720p & 480p to trigger rampdowns.
"""
simlinearResp = [
        {"status": 404, "pattern": "(1080|720|480)p.m3u8"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')


initRampdownLimit = 2
TESTDATA1 = {
    "title": "initRampdownLimit",
    "logfile": f"initRampdownLimit{initRampdownLimit}.txt",
    "max_test_time_seconds": 30,
    # set initial bitrate = 5000000 to start 1080p profile 
    "aamp_cfg": f"info=true\ntrace=true\ninitRampdownLimit={initRampdownLimit}\ninitialBitrate=5000000",
    "archive_url": archive_url,
    "url":f"VideoTestStream_HLS/main.m3u8?respData={data}",
    "simlinear_type": "HLS",
    "expect_list": [
        # Check AAMP Status changed to "INITIALIZING"
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        # Http 404 received for 1080p playlist
        {"expect": r"Playlist download failed : http://localhost:8085/VideoTestStream_HLS/hls/1080p.m3u8\?respData=" + data + r"  http response : 404"},
        # Attemped 1st rampdown to 720p playlist
        {"expect": r"Video playlist download failed, rettrying with rampdown logic : 1 \( 2 \)"},
        # Http 404 received for 720p playlist
        {"expect": r"Playlist download failed : http://localhost:8085/VideoTestStream_HLS/hls/720p.m3u8\?respData=" + data + r"  http response : 404"},
        # Attemped 2nd rampdown to 480p playlist
        {"expect": r"Video playlist download failed, rettrying with rampdown logic : 2 \( 2 \)"},
        # Http 404 received for 480p playlist
        {"expect": r"Playlist download failed : http://localhost:8085/VideoTestStream_HLS/hls/480p.m3u8\?respData=" + data + r"  http response : 404"},
        # Attemped 3rd rampdown to 360p playlist, since exceeded the ramdownlimit, tune failed event raised.
        {"expect": "Video playlist download failed still after 3 rampdown attempts"},
        {"expect": r"init failed \(unable to download video playlist\) : Http Error Code 404"},
        {"expect": r"AAMP_EVENT_TUNE_FAILED reason=AAMP: init failed \(unable to download video playlist\) : Http Error Code 404"},
    ]
}

initRampdownLimit = 1
TESTDATA2 = {
    "title": "initRampdownLimit",
    "logfile": f"initRampdownLimit{initRampdownLimit}.txt",
    "max_test_time_seconds": 30,
    # set initial bitrate = 5000000 to start 1080p profile 
    "aamp_cfg": f"info=true\ntrace=true\ninitRampdownLimit={initRampdownLimit}\ninitialBitrate=5000000",
    "archive_url": archive_url,
    "url":f"VideoTestStream_HLS/main.m3u8?respData={data}",
    "simlinear_type": "HLS",
    "expect_list": [
        # Check AAMP Status changed to "INITIALIZING"
        {"expect": "AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        # Http 404 received for 1080p playlist
        {"expect": r"Playlist download failed : http://localhost:8085/VideoTestStream_HLS/hls/1080p.m3u8\?respData=" + data + r"  http response : 404"},
        # Attemped 1st rampdown to 720p playlist
        {"expect": r"Video playlist download failed, rettrying with rampdown logic : 1 \( 1 \)"},
        # Http 404 received for 720p playlist
        {"expect": r"Playlist download failed : http://localhost:8085/VideoTestStream_HLS/hls/720p.m3u8\?respData=" + data + r"  http response : 404"},
        # Attemped 2nd rampdown to 480p playlist, since exceeded the ramdownlimit, tune failed event raised.
        {"expect": "Video playlist download failed still after 2 rampdown attempts"},
        {"expect": r"init failed \(unable to download video playlist\) : Http Error Code 404"},
        {"expect": r"AAMP_EVENT_TUNE_FAILED reason=AAMP: init failed \(unable to download video playlist\) : Http Error Code 404"},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2031(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
