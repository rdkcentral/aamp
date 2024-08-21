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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events


import os
import re
import pytest

from inspect import getsourcefile

#Test stream
MULTI_TEST_STREAM = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/multi-period/multi-audio-codec/codec.mpd"
#common aamp.cfg
CFG_STR="info=true\ntrace=true\nprogress=true\n"

TESTDATA1 = {
    "title": "codec_change_eos",
    "logfile": "codec_change_eos.log",
    "max_test_time_seconds": 100,
    "aamp_cfg":  CFG_STR+"enablePTSReStamp=true\n",
    "url": MULTI_TEST_STREAM,
    "expect_list":
    [
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"Schedule retune for GstPipeline Error","min":1, "max":99,"not_expected" : True},
        {"expect" : r"PTS-RESTAMP ENABLED, but we have codec change, so Signal EOS","min":10, "max":100,"end_of_test":True},
    ]
}

TESTDATA2 = {
    "title": "codec_change_eos_pts_off",
    "logfile": "codec_change_eos_pts_off.log",
    "max_test_time_seconds": 100,
    "aamp_cfg": CFG_STR+"enablePTSReStamp=false\n",
    "url": MULTI_TEST_STREAM,
    "expect_list":
    [
        {"expect": r"aamp_tune","min":0, "max":1},
        {"expect" : r"Schedule retune for GstPipeline Error","min":1, "max":99,"not_expected" : True},
        {"expect" : r"playing period \d+/13","min":30, "max":100,"end_of_test":True},
    ]
}

TESTDATA = [TESTDATA1,TESTDATA2]

@pytest.fixture(params=TESTDATA)
def test_data(request):
    return request.param

def test_13005(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
