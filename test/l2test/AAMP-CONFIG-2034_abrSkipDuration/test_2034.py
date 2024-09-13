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

TESTDATA1 = {
    "title": "Test case to validate abrSkipDuration",
    "logfile": "abrSkipDuration_8.txt",
    "max_test_time_seconds": 20,
    "url":"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nabrSkipDuration=8",
    "expect_list": [
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/480p_004.m4s "},
        {"expect" : r"getProfileIndexByBitrateRampUpOrDown:580 currBW:1400000 NwBW=[0-9]{8,9} currProf:2 desiredProf:0 Period ID:"},
        {"expect" : r"switching to 'higher' profile '2 -> 0' currentBandwidth\[1400000\]->desiredBandwidth\[5000000\]"},
        {"expect" : r"ABR 842x474\[1400000] -> 1920x1080\[5000000]"},
        {"expect" : r"cachedFragment->position: 8\.[0-9]{6} cachedFragment->duration: [0-9]\.[0-9]{6}"},
        {"expect" : r"NotifyBitRateChangeEvent \:: bitrate:5000000 desc:BitrateChanged - Network adaptation width:1920"},
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_005.m4s"},
        {"expect" : r"video - injected cached uri at pos 8\.[0-9]{6} dur [0-9]\.[0-9]{6}"},
        {"expect" : r"Returning Position as [1-3][0-9]{3}"},
        {"expect" : r"Returning Position as [4-6][0-9]{3}"},
    ],
}

TESTDATA2 = {
    "title": "Test case to validate abrSkipDuration",
    "logfile": "abrSkipDuration_4.txt",
    "max_test_time_seconds": 40,
    "url":"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nabrSkipDuration=4",
    "expect_list": [
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/480p_002.m4s "}, 
        {"expect" : r"getProfileIndexByBitrateRampUpOrDown:580 currBW:1400000 NwBW=[0-9]{8,9} currProf:2 desiredProf:0 Period ID:"}, 
        {"expect" : r"switching to 'higher' profile '2 -> 0' currentBandwidth\[1400000\]->desiredBandwidth\[5000000\]"}, 
        {"expect" : r"ABR 842x474\[1400000] -> 1920x1080\[5000000]"}, 
        {"expect" : r"cachedFragment->position: [2-4]\.[0-9]{6} cachedFragment->duration: [0-9]\.[0-9]{6}"}, 
        {"expect" : r"NotifyBitRateChangeEvent :: bitrate:5000000 desc:BitrateChanged - Network adaptation width:1920"}, 
        {"expect" : r"video - injected cached uri at pos 4\.[0-9]{6} dur 2\.[0-9]{6}"}, 
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_004.m4s"}, 
        {"expect" : r"video - injected cached uri at pos 8\.000000 dur [0-9]\.000000"}, 
        {"expect" : r"Returning Position as [1-3][0-9]{3}"}, 
        {"expect" : r"Returning Position as [4-6][0-9]{3}"}, 
    ],
}

TESTDATA3 = {
    "title": "Test case to validate abrSkipDuration",
    "logfile": "abrSkipDuration_10.txt",
    "max_test_time_seconds": 40,
    "url":"VideoTestStream/main.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nabrSkipDuration=10",
    "expect_list": [
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/480p_005.m4s "},
        {"expect" : r"getProfileIndexByBitrateRampUpOrDown:580 currBW:1400000 NwBW=[0-9]{8,9} currProf:2 desiredProf:0 Period ID:"},
        {"expect" : r"switching to 'higher' profile '2 -> 0' currentBandwidth\[1400000\]->desiredBandwidth\[5000000\]"},
        {"expect" : r"ABR 842x474\[1400000] -> 1920x1080\[5000000]"},
        {"expect" : r"cachedFragment->position: [0-9]{1,2}\.[0-9]{6} cachedFragment->duration: [0-9]\.[0-9]{6}"},
        {"expect" : r"NotifyBitRateChangeEvent \:: bitrate:5000000 desc:BitrateChanged - Network adaptation width:1920"},
        {"expect" : r"video - injected cached uri at pos [0-9]{1,2}\.[0-9]{6} dur [0-9]\.[0-9]{6}"},
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/1080p_006.m4s"},
        {"expect" : r"Returning Position as [1-3][0-9]{3}"},
        {"expect" : r"Returning Position as [4-6][0-9]{3}"},
    ],
}

"""
[{"status": 404, "pattern": "(1080|720)p_"}]
Forcing 404 error for 1080p & 720p so that abrSkipDuration will be triggered and playback will still remain on 480p only.
"""
simlinearResp = [
        {"status": 404, "pattern": "(1080|720)p_"}
    ]
data = base64.b64encode(json.dumps(simlinearResp).encode('utf-8')).decode('utf-8')
abrSkipDuration = 10
TESTDATA4 = {
    "title": "Test case to validate abrSkipDuration",
    "logfile": f"abrSkipDuration_480p_{abrSkipDuration}.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nabrSkipDuration={abrSkipDuration}\n",
    "url":f"VideoTestStream/main.mpd?respData={data}",
    "simlinear_type": "DASH",
    "expect_list": [ 
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/480p_005.m4s"},
        {"expect" : r"getProfileIndexByBitrateRampUpOrDown:580 currBW:1400000 NwBW=[0-9]{8,9} currProf:2 desiredProf:0 Period ID:"},
        {"expect" : r"switching to 'higher' profile '2 -> 0' currentBandwidth\[1400000\]->desiredBandwidth\[5000000\]"},
        {"expect" : r"ABR 842x474\[1400000] -> 1920x1080\[5000000]"},
        {"expect" : r"(?i)AAMPLogNetworkError error='http error 404' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/1080p_init\.m4s\?respData="+data+"'"},
        {"expect" : r"AAMPLogABRInfo : switching to 'lower' profile '0 -> 1' currentBandwidth\[5000000\]->desiredBandwidth\[2800000\] nwBandwidth\[-1\] reason='fragment download failed' error='http error 404' symptom='video quality may decrease \(or\) freeze/buffering'"},
        {"expect" : r"ABR 1920x1080\[5000000\] -> 1280x720\[2800000\]"},
        {"expect" : r"(?i)AAMPLogNetworkError error='http error 404' type='INIT_VIDEO' location='unknown' symptom='video fails to start' url='http://localhost:8085/VideoTestStream/dash/720p_init\.m4s\?respData="+data+"'"},
        {"expect" : r"AAMPLogABRInfo : switching to 'lower' profile '1 -> 2' currentBandwidth\[2800000\]->desiredBandwidth\[1400000\] nwBandwidth\[-1\] reason='fragment download failed' error='http error 404' symptom='video quality may decrease \(or\) freeze/buffering'"},
        {"expect" : r"ABR 1280x720\[2800000\] -> 842x474\[1400000\]"},
        {"expect" : r"cachedFragment->position: [0-9]{1,2}\.[0-9]{6} cachedFragment->duration: [0-9]\.[0-9]{6}"},
        {"expect" : r"video - injected cached uri at pos [0-9]{1,2}\.[0-9]{6} dur [0-9]\.[0-9]{6}"},
        {"expect" : r"fragmentUrl http://localhost:8085/VideoTestStream/dash/480p_006\.m4s"},
        {"expect" : r"Returning Position as [1-3][0-9]{3}"},
        {"expect" : r"Returning Position as [4-6][0-9]{3}"},
    ]
}
TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]



############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2034(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
