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

"""
[{"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}]
Gives HTTP 404 error for all video segments numbered from 006 to 018
"""
simlinearResp = [
        {"status": 404, "pattern": "(480|1080|360|720)p_0(0[6-9]|1[0-8]).m4s"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')


TESTDATA1 = {
    "title": "Fragment Download Fail Threshold",
    "logfile": f"single_video_track.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nfragmentDownloadFailThreshold=8\n",
    "url":"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"cmd": 'setconfig {"propagateUriParameters":false}'},
        {"cmd": 'setconfig {"uriParameter":"?respData='+data+'","curlHeader":true}'},
        {"expect": "Parsed value for property propagateUriParameters - false"},
        {"cmd": "set videoTrack 800000 800000 800000"},
        {"cmd": "http://localhost:8085/VideoTestStream/main.mpd"},
        {"expect": r"VideoTestStream/dash/(480|1080|360|720)p_005\.m4s"},
        {"cmd": 'setconfig {"propagateUriParameters":true}'},
        {"expect": "Parsed value for property propagateUriParameters - true"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": r"Not\ able\ to\ download\ (init\s)?fragments;\ reached\ failure\ threshold\ sending\ tune\ failed\ event"},
        {"expect": re.escape("[video] Playlist downloader thread not started")},
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: fragment download failures"},
    ]
}

"""
[{"status": 404, "pattern": "en_0(0[6-9]|1[0-8]).mp3"}]
Gives HTTP 404 error for all english audio segments numbered from 006 to 018
"""
simlinearResp = [
        {"status": 404, "pattern": "en_0(0[6-9]|1[0-8]).mp3"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')

TESTDATA2 = {
    "title": "Fragment Download Fail Threshold",
    "logfile": f"only_audio.txt",
    "max_test_time_seconds": 10,
    "aamp_cfg": "info=true\ntrace=true\nabr=false\naudioOnlyPlayback=true\nfragmentDownloadFailThreshold=4\n",
    "url":"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"cmd": 'setconfig {"propagateUriParameters":false,"uriParameter":"?respData='+data+'","curlHeader":true}'},
        {"expect": "Parsed value for property propagateUriParameters - false"},
        {"cmd": "http://localhost:8085/VideoTestStream/main.mpd"},
        {"expect": r"VideoTestStream/dash/en_003\.mp3"},
        {"cmd": 'setconfig {"propagateUriParameters":true}'},
        {"expect": "Parsed value for property propagateUriParameters - true"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": "fragment fetch failed -- fragmentUrl"},
        {"expect": r"Not\ able\ to\ download\ (init\s)?fragments;\ reached\ failure\ threshold\ sending\ tune\ failed\ event"},
        {"expect": re.escape("[video] Playlist downloader thread not started")},
        {"expect": "AAMP_EVENT_TUNE_FAILED reason=AAMP: fragment download failures"},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2]

############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2035(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)