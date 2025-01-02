#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

simlinearResp0 = [
        {"status": 502, "start": 5, "end": 10, "pattern": "SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163\\.mpd.*"}
    ]
data0 = base64.b64encode(json.dumps(simlinearResp0).encode('utf-8')).decode('utf-8')

simlinearResp1 = [
	{"status": 502, "start": 2, "end": 10, "pattern": r"track-(video|audio)-periodid-\d+-repid-root_(audio\d+|video\d+)-tc-\d+-frag-\d+\.mp4"}
    ]
data1 = base64.b64encode(json.dumps(simlinearResp1).encode('utf-8')).decode('utf-8')

simlinearResp2 = [
        {"status": 502, "start": 5, "end": 10}
    ]
data2 = base64.b64encode(json.dumps(simlinearResp2).encode('utf-8')).decode('utf-8')

simlinearResp3 = [
        {"status": 502, "start": 2, "end": 25}
    ]
data3 = base64.b64encode(json.dumps(simlinearResp3).encode('utf-8')).decode('utf-8')

#Network flap test for manifest retry
TESTDATA0 = {
    "title": "Networkflap Test for manifest retry",
    "logfile": "networkflapmanifest0.log",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\nprogress=true\ntrace=true\n",
    "archive_url": archive_url,
    "url": f"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd?respData={data0}",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"aamp_tune"},
        {"expect": r"Download failed due to Server error http-502"},
        {"expect": r"Download Status Ret:0 502"},
        {"expect": r"FOREGROUND PLAYER\[0\] Sending error AAMP:", "not_expected": True},
        #Check nothing in the body of the html gets passed to parser when we get 502
        {"expect": r"parser error : Document is empty", "not_expected": True},
        # Recovery follows
        {"expect": r"Parse MPD Completed ...", "min":11},
        {"expect": r"Successfully parsed Manifest ...IsLive", "min":11},
        {"expect": r"Download Status Ret:0 200", "min":11},
        {"expect": r"HttpRequestEnd: 3,4,200", "min":11},
        {"expect": r"Returning Position as 16916026(\d{5})","min":11},
        {"expect": r"HttpRequestEnd: 0,0,200","min":15, "end_of_test": True},
    ]
}

#Network flap test for fragment retry and rampdown skip
TESTDATA1 = {
    "title": "Networkflap Test for fragment retry and rampdown skip",
    "logfile": "networkflapfragment1.log",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\nprogress=true\ntrace=true\n",
    "archive_url": archive_url,
    "url": f"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd?respData={data1}",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"aamp_tune"},
        {"expect": r"Download failed due to Server error. Retrying Attempt:1!"},
        {"expect": r"AAMPLogNetworkError error='http error 502' type='video' location='unknown' symptom='freeze/buffering'"},
        {"expect": r"HttpRequestEnd: 0,0,502"},
        {"expect": r"StreamAbstractionAAMP: Condition Rampdown Success","not_expected": True},
        {"expect": r"FOREGROUND PLAYER\[0\] Sending error AAMP:","not_expected" : True},
        # Recovery follows
        {"expect": r"HttpRequestEnd: 0,0,200", "min":11},
        {"expect": r"Download Status Ret:0 200", "min":11},
        {"expect": r"Returning Position as 16916026(\d{5})","min":15},
        {"expect": r"aamp pos: \[.*?\.\.1\.00\]", "min": 15, "end_of_test": True},
    ]
}

#Network flap test for fragment and manifest retry, 5 sec failure for both
TESTDATA2 = {
    "title": "Networkflap Test for fragment and manifest retry",
    "logfile": "networkflapmanifestandfragment2.log",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\nprogress=true\ntrace=true\n",
    "archive_url": archive_url,
    "url": f"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd?respData={data2}",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"aamp_tune"},
        {"expect": r"Download Status Ret:0 502"},
        {"expect": r"Download failed due to Server error http-502"},
        {"expect": r"FOREGROUND PLAYER\[0\] Sending error AAMP:","not_expected" : True},
        # Recovery follows
        {"expect": r"Download Status Ret:0 200", "min":11},
        {"expect": r"HttpRequestEnd: 0,0,200", "min":11},
        {"expect": r"Returning Position as 16916026(\d{5})","min":11},
        {"expect": r"HttpRequestEnd: 0,0,200","min":15, "end_of_test": True},
    ]
}

#Network flap test for fragment and manifest retry not recovering case.
TESTDATA3= {
    "title": "Networkflap Test for fragment and manifest retry not recovering case",
    "logfile": "networkflapmanifestandfragmentnotrecovering3.log",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\nprogress=true\ntrace=true\n",
    "archive_url": archive_url,
    "url": f"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd?respData={data3}",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"aamp_tune"},
        {"expect": r"HttpRequestEnd: 0,0,200"},
        {"expect": r"aamp pos:.*" + re.escape("..1.00]")},
        {"expect": r"Download failed due to Server error http-502"},
        {"expect": r"Download Status Ret:0 502"},
        {"expect": r"aamp pos:.*" + re.escape("..1.00]"),"not_expected" : True, "min":20}, #Dont expect to see this after 20sec into test
        {"expect": r"FOREGROUND PLAYER\[0\] Sending error AAMP: Manifest Download failed : Http Error Code 502","end_of_test": True},
    ]
}

############################################################
"""
With this fixture we cause the test to be called with each entry in TESTLIST
"""
TESTLIST = [TESTDATA0,TESTDATA1,TESTDATA2,TESTDATA3]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_9000(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

