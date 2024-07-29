
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
TESTDATA1 = {
    "title": "Test case to validate SetLiveOffset() API",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 15,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset=10",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]")},
        {"expect": "Live offset value updated to 10"},
        {"expect": "offsetFromStart 18.800000"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
    ]
}
TESTDATA2 = {
    "title": "Test case to validate SetLiveOffset() API",
    "logfile": "testdata2.txt",
    "max_test_time_seconds": 10,
     "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset=0",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]")},
        {"expect": r"Live offset value updated to 0"}, 
        {"expect": r"offsetFromStart 28.800000"}, 
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
    ],
}
TESTDATA3 = {
    "title": "Test case to validate SetLiveOffset() API",
    "logfile": "testdata3.txt",
    "max_test_time_seconds": 10,
     "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset=15",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]")},
        {"expect": r"Live offset value updated to 15"}, 
        {"expect": r"offsetFromStart 13.800000"}, 
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
    ],
}

TESTLIST = [TESTDATA1 ,TESTDATA2 ,TESTDATA3 ]
############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
def test_2017(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
