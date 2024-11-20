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

# TSB L2 initial Tests


import os
import sys
import json
import copy
import pytest
from inspect import getsourcefile

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/ch920_10min.tar.xz"

# // Test URL with Low Latency Content
LLD_URL="v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd?chunked"

# // TODO replace with live SLD URL
SLD_URL="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd"

# Test Session Manager initialization with config true
TESTDATA1 = {
    "title": "configtrue",
    "logfile": "configtrue.log",
    "max_test_time_seconds": 15,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "archive_url": archive_url,
    "url": LLD_URL,
    'simlinear_type': 'DASH',
    "expect_list":
    [
        {"expect" : r"\[TSB Store\] Initiating with config values","min":0, "max":1},
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"msg=\"File written\"","min":0, "max":5 ,"end_of_test":True},
    ]
}

# Test Session Manager not initialized with config false
TESTDATA2 = {
    "title": "configfalse",
    "logfile": "configfalse.log",
    "max_test_time_seconds": 15,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=false\nprogress=true\ntsbLocation=/tmp/data\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "archive_url": archive_url,
    "url": LLD_URL,
    'simlinear_type': 'DASH',
    "expect_list":
    [
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"\[TSB Store\] Initiating TSBStore with config values","min":0, "max":3,"not_expected" : True},
        {"expect": r"first buffer received","min":0, "max":10, "end_of_test":True},
    ]
}

# Test for TSB Culling logs
TESTDATA3 = {
    "title": "Culling ",
    "logfile": "culling.log",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=6\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "archive_url": archive_url,
    "url": LLD_URL,
    'simlinear_type': 'DASH',
    "expect_list":
    [
        {"expect" : r"\[TSB Store\] Initiating with config values","min":0, "max":1},
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"TSBWrite Metrics...OK","min":1, "max":40},
        {"expect" : r"TSB Write Operation FAILED","min":0, "max":8,"not_expected" : True},
        {"expect" : r"CullSegments","min":1, "max":40},
        {"expect" : r"Removed \d.\d+ fragment duration seconds","min":8, "max":40 ,"end_of_test":True},
    ]
}

# Test TSB Data Manager basic logs
TESTDATA4 = {
    "title": "Data Manager ",
    "logfile": "datamgr.log",
    "max_test_time_seconds": 20,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\ntsbLength=4\nlldUrlKeyword=chunked\n",
    "archive_url": archive_url,
    "url": LLD_URL,
    'simlinear_type': 'DASH',
    "expect_list":
    [
        {"expect" : r"\[TSB Store\] Initiating with config values","min":0, "max":1},
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"Adding Init Data:","min":0, "max":10},
        {"expect" : r"Adding Fragment Data: ","min":0, "max":10},
        {"expect" : r"TSBWrite Metrics...OK","min":3, "max":10},
        {"expect" : r"TSB Write Operation FAILED","min":3, "max":20,"not_expected" : True},
        {"expect" : r"Removed \d.\d+ fragment duration seconds","min":8, "max":20 ,"end_of_test":True},
    ]
}

# Test for TSB Store logs
TESTDATA5 = {
    "title": "TSB Library ",
    "logfile": "tsblib.log",
    "max_test_time_seconds": 35,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=4\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "archive_url": archive_url,
    "url": LLD_URL,
    'simlinear_type': 'DASH',
    "expect_list":
    [
        {"expect" : r"\[TSB Store\] Initiating with config values","min":0, "max":1},
        {"expect" : r"minFreePercentage : \d+","min":0, "max":1},
        {"expect" : r"msg=\"Flusher thread running\"","min":0, "max":1},
        {"expect" : r"msg=\"Flush storage content\"","min":0, "max":1},
        {"expect" : r"msg=\"Store Constructed\"","min":0, "max":1},
        {"expect" : r"msg=\"File written\" ","min":0, "max":10},
        {"expect" : r"msg=\"Deleted file\" ","min":0, "max":10},
        {"expect" : r"TSBWrite Metrics...OK","min":0, "max":10},
        {"expect" : r"TSB Write Operation FAILED","min":0, "max":10,"not_expected" : True},
        {"expect" : r"Removed \d.\d+ fragment duration seconds","min":8, "max":20 ,"end_of_test":True},
    ]
}

# Test if Playback starts with chunked word passed alongwith Non LLD URL
TESTDATA6 = {
    "title": "Non LLD + Chunked",
    "logfile": "nonlldchunked.log",
    "max_test_time_seconds": 20,
    "aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=12\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "url": SLD_URL+"?chunked=/low/",
    "expect_list":
    [
        {"expect" : r"aamp_tune","min":0, "max":1},
        {"expect" : r"crashed|failure","min":0, "max":2,"not_expected" : True},
        {"expect" : r"aamp pos","min":6, "max":20 ,"end_of_test":True},
    ]
}

# // Read API Test
TESTDATA_read = {
    "title": "Test Read API",
    "logfile": "readapi.log",
    "max_test_time_seconds":    60,
    'simlinear_type': 'DASH',
    "archive_url": archive_url,
    "url": LLD_URL,
    "aamp_cfg": "progress=true\ninfo=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
    "expect_list":
    [
        {"cmd": "sleep 20000"},
        {"expect": r"sleep complete"},
        {"cmd": "seek 0"},
        {"expect": r"aamp_Seek\(0.000000\)"},
        {"expect": r"msg=\"Got size\""},
        {"expect": r"File Read"},
        {"cmd": "sleep 5000"},
        {"expect": r"sleeping"},
        {"cmd": "exit"}
    ]
}

TESTDATA = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5,TESTDATA6]

@pytest.fixture(params=TESTDATA)
def test_data(request):
    return request.param

def test_5001(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

def test_5001_2(aamp_setup_teardown):

     aamp = aamp_setup_teardown
     aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
     aamp.run_expect_a(TESTDATA_read)
