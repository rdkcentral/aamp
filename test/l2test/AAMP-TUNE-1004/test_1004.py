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

# Starts simlinear, a web server for serving test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
# Also see README.md

import os
import sys
from inspect import getsourcefile
import pytest

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

TESTDATA0 = {
"title": "Tune time test 0",
"archive_url": archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\n",
"cmdlist": [
    ],
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": r"aamp_tune","min":0, "max":2},
    {"expect": r"PAUSED -> PLAYING","min":0, "max":2},
    {"expect": r"Stopping Playback","min":0, "max":2},
    {"expect": r"SendTuneMetricsEvent","min":0, "max":2},
    {"expect": r"NotifyFirstFrameReceived","min":0, "max":2, "end_of_test":True},
    ]
}

def test_1004_0(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA0)



