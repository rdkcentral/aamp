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
    "title": "Test case to validate live command",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 40,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=0",
    "expect_list": [
        {"expect": r"Updated live offset for 4K stream 0","min":0,"max":1,"end_of_test":True}, 
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"min":0,"max":10,"end_of_test":True},
        {"expect": r"Returning Position as 1689262831(\d{3})"}, 
        {"cmd" : "pause"},
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PAUSED"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"},
        {"cmd": "sleep 5000"}, 
        {"expect": "sleep complete"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"}, 
        {"cmd": "sleep 5000"}, 
        {"expect": "sleep complete"},
        {"cmd" : "live"}, 
        {"expect" : r"aamp_Seek position adjusted to absolute value: 1689262798\.[0-9]{6}"}, 
        {"expect" : r"eTUNETYPE_SEEKTOLIVE"}, 
        {"expect" : r"Updated seek_pos_seconds 168926284[4-8]"} , 
        {"expect" : r"TuneHelper - seek_pos: 168926284[4-8]"},
        {"expect" : r"AAMP_EVENT_SEEKED: new positionMs 168926284[4-8]"},
        # {"cmd": "sleep 3000"},
        # {"expect": "sleep complete"} 
    ],
}

TESTDATA2 = {
    "title": "Test case to validate live command",
    "logfile": "testdata2.txt",
    "max_test_time_seconds": 40,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=0",
    "expect_list": [
        {"expect": r"Updated live offset for 4K stream 0","min":0,"max":1,"end_of_test":True}, 
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"min":0,"max":10,"end_of_test":True},
        {"expect": r"Returning Position as 1689262831(\d{3})"}, 
        {"cmd" : "pause"},
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PAUSED"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"},
        {"cmd": "sleep 3000"}, 
        {"expect": "sleep complete"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"}, 
        {"cmd": "sleep 3000"}, 
        {"expect": "sleep complete"},
        {"cmd" : "live"}, 
        {"expect" : r"aamp_Seek position adjusted to absolute value: 1689262798\.[0-9]{6}"}, 
        {"expect" : r"eTUNETYPE_SEEKTOLIVE"}, 
        {"expect" : r"Updated seek_pos_seconds 16892628(3[6-9]|4[0-8])"} , 
        {"expect" : r"TuneHelper - seek_pos: 16892628(3[6-9]|4[0-8])"},
        # {"expect" : r"Returning Position as 1689262837(\d{3})"},
        {"expect" : r"AAMP_EVENT_SEEKED: new positionMs 16892628(3[6-9]|4[0-8])"},
        # {"cmd": "sleep 5000"},
        # {"expect": "sleep complete"} 
    ],
}
TESTDATA3 = {
    "title": "Test case to validate live command",
    "logfile": "testdata3.txt",
    "max_test_time_seconds": 40,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": "info=true\ntrace=true\nliveOffset4K=0",
    "expect_list": [
        {"expect": r"Updated live offset for 4K stream 0","min":0,"max":1,"end_of_test":True}, 
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]"),"min":0,"max":10,"end_of_test":True},
        {"expect": r"Returning Position as 1689262831(\d{3})"}, 
        {"cmd" : "pause"},
        {"expect" : r"AAMP_EVENT_STATE_CHANGED: PAUSED"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"},
        {"cmd": "sleep 10000"}, 
        {"expect": "sleep complete"}, 
        {"expect" : r"Returning Position as 1689262831(\d{3})"},
        {"cmd": "sleep 10000"}, 
        {"expect": "sleep complete"},
        {"cmd" : "live"}, 
        {"expect" : r"aamp_Seek position adjusted to absolute value: 1689262798\.[0-9]{6}"}, 
        {"expect" : r"eTUNETYPE_SEEKTOLIVE"}, 
        {"expect" : r"Updated seek_pos_seconds 168926285[0-9]"} , 
        {"expect" : r"TuneHelper - seek_pos: 168926285[0-9]"}, 
        # {"expect" : r"Returning Position as 1689262853(\d{3})"},
        {"expect" : r"AAMP_EVENT_SEEKED: new positionMs 168926285[0-9]"},
        # {"cmd": "sleep 5000"},
        # {"expect": "sleep complete"} 
    ],
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2033(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


