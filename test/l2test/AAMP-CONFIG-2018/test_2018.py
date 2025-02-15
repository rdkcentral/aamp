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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/sky/skywitness-4klive-8M.tar.gz"

TESTDATA1 = {
    "title": "Test case to validate SetLiveOffset4K() API",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 10,
    "archive_url": archive_url,
     "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=0",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"max":10,"end_of_test":True},
        {"expect": r"Updated live offset for 4K stream 0","max":1,"end_of_test":True}, 
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"cmd": "sleep 1000"},
        {"expect": "sleeping for 1.000000 seconds"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
    ],
}

TESTDATA2 = {
    "title": "Test case to validate SetLiveOffset4K() API",
    "logfile": "testdata2.txt",
    "max_test_time_seconds": 10,
    "archive_url": archive_url,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=15",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"max":10,"end_of_test":True},
        {"expect": r"Updated live offset for 4K stream 15","max":5,"end_of_test":True}, 
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"cmd": "sleep 1000"},
        {"expect": "sleeping for 1.000000 seconds"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},


    ],
}

TESTDATA3 = {
    "title": "Test case to validate SetLiveOffset4K() API",
    "logfile": "testdata3.txt",
    "max_test_time_seconds": 10,
    "archive_url": archive_url,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=10",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"max":10,"end_of_test":True},
        {"expect": r"Updated live offset for 4K stream 10","max":5,"end_of_test":True}, 
        {"expect": r"Returning Position as 16892628(\d{5})"},
        {"cmd": "sleep 1000"},
        {"expect": "sleeping for 1.000000 seconds"},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
        {"expect": r"Returning Position as 16892628(\d{5}) "},
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

def test_2018(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


