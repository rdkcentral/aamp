#  !/usr/bin/env python3
#  If not stated otherwise in this file or this component's LICENSE file the
#  following copyright and licenses apply:
#  Copyright 2024 RDK Management
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#  http://www.apache.org/licenses/LICENSE-2.0
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

from inspect import getsourcefile
import os
import pytest


archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CONFIG-2072/LL-DashDownloadBuffer.tar.xz"
# Define test data configurations
TESTDATA0 = {
    "title": "Set EnableLowLatencyCorrection to true",
    "logfile": "enableLowLatencyCorrection_true.log",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\nenableLowLatencyCorrection=true\n",
    "max_test_time_seconds": 30,
    "archive_url": archive_url,
    "url": "LL-Dash/cdn-vos-ppp-01.vos360.video/Content/DASH_DASHCLEAR2/Live/channel(PPP-LL-2DASH)/manifest.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"mManifestUrl: http://localhost:8085/LL-Dash/cdn-vos-ppp-\d+.vos360.video/Content/DASH_DASHCLEAR2/Live/channel\([A-Za-z0-9\-]+\)/manifest\.mpd"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
        {"expect": r"LL-Dash speed correction enabled"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
        {"expect": r"Speed correction state:1"},
        {"expect": r"currentLatency = (-?\d+\.\d\d)  AvailableBuffer = \d+\.\d+ minbuffer = \d+\.\d+ targetBuffer=\d+\.\d+ currentPlaybackRate = \d+\.\d+ bufferLowHitted = \d isEnoughBuffer = \d latencyCorrected = \d bufferCorrectionStarted = \d"},
        {"expect": r"Returning Position as [0-6](\d{4})"},
    ],
}

TESTDATA1 = {
    "title": "Set EnableLowLatencyCorrection to false",
    "logfile": "enableLowLatencyCorrection_false.log",
    "aamp_cfg": "trace=true\ninfo=true\nprogress=true\nenableLowLatencyCorrection=false\n",
    "max_test_time_seconds": 30,
    "archive_url": archive_url,
    "url": "LL-Dash/cdn-vos-ppp-01.vos360.video/Content/DASH_DASHCLEAR2/Live/channel(PPP-LL-2DASH)/manifest.mpd",
    "simlinear_type": "DASH",
    "expect_list": [
        {"expect": r"mManifestUrl: http://localhost:8085/LL-Dash/cdn-vos-ppp-\d+.vos360.video/Content/DASH_DASHCLEAR2/Live/channel\([A-Za-z0-9\-]+\)/manifest\.mpd","max": 5},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: INITIALIZING", "max": 5},
        {"expect": r"LL-Dash speed correction disabled", "max": 5},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING", "max": 5},
        {"expect": r"currentLatency = (-?\d+\.\d\d)  AvailableBuffer = \d+\.\d+ minbuffer = \d+\.\d+ targetBuffer=\d+\.\d+ currentPlaybackRate = \d+\.\d+ bufferLowHitted = \d isEnoughBuffer = \d latencyCorrected = \d bufferCorrectionStarted = \d","not_expected": True},
        {"expect": r"Returning Position as [0-6](\d{4})","min": 18,"end_of_test": True},
    ],
}

############################################################

def test_2053_0(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA0)

def test_2053_1(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA1)
