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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events

from inspect import getsourcefile
import os
import pytest
import re

# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-MV-5000/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.tar.gz"

TESTDATA0 = {
    "title": "CDAI Single Pipeline - Multiple Assets",
    "max_test_time_seconds": 60,

    "expect_list":
    [
        {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json
        # Create main content player - Player 1
        {"cmd":"new"},
        {"expect":r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},

        # Toggle autoplay off
        {"cmd":"autoplay"},

        {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd":"set videoTrack 5000000 5000000 5000000"},
        {"expect": "bitrate = 5000000" },
        {"expect": "HttpRequestEnd: 0,0,200.*1080p"}
    ]
}

TESTDATA1 = {
    "title": "CDAI Single Pipeline - Multiple Assets",
    "max_test_time_seconds": 60,
    "archive_url": archive_url,
    "simlinear_type": "DASH",
    "expect_list":
    [
        {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json
        # Create main content player - Player 1
        {"cmd":"new"},
        {"expect":r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},

        # Toggle autoplay off
        {"cmd":"autoplay"},

        {"cmd":"http://localhost:8085/f2decf88-72cb-4e81-8e7b-ae230ea5c83b/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd"},
        {"cmd":"set videoTrack 4461200 4461200 4461200"},
        {"expect": "HttpRequestEnd: 0,0,200,.*4461200*"}
    ]
}

############################################################
TESTLIST = [TESTDATA0, TESTDATA1]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

@pytest.mark.ci_test_set
def test_1010(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
