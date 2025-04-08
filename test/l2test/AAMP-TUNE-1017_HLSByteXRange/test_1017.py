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

# Starts aamp-cli and initiates playback by giving it a DASH stream URL
# Stops stream after ~1s of playback, and ensure that player state flows through STOPPING, with RELEASED state reached after all resources actually released
# verifies aamp log output against expected list of events
# Also see README.md

from inspect import getsourcefile
import os
import pytest
import re

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/byterange-test-stream.zip"
TESTDATA0 = {
"title": "HLS playback",
"max_test_time_seconds": 20,
"aamp_cfg": "info=true\nprogress=true\njsinfo=true",
"archive_url": archive_url,
"url": "frag_mp4_byterange.m3u8",
"simlinear_type": "HLS",
"expect_list": [
    #Check the progress of playback
   {"expect": r"aamp pos: \[(.*)" + re.escape("0..4..10..") + r"(.*)]"},
   {"expect": r"aamp pos: \[(.*)" + re.escape("0..8..10..") + r"(.*)]"},
   #Check for EOS
   {"expect": r"GST_MESSAGE_EOS"}
]
}


TESTLIST = [TESTDATA0]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

@pytest.mark.ci_test_set
def test_1015(aamp_setup_teardown, test_data):

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
