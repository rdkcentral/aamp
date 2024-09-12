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

import os
import platform
from inspect import getsourcefile
import pytest
import re 


TESTDATA1 = {
"title": "Test corrupt fragments",
"logfile": "test.log",
"max_test_time_seconds": 30,
"aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
"url":"content/main.mpd",
"simlinear_type": "DASH",
"expect_list": [
    {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8085/content/main.mpd"},
    {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:0 msg:Error\[28\]:GstPipeline Error:This file is invalid and cannot be played."},
    {"expect": r"GST_MESSAGE_ERROR source: Internal data stream error."},
]
}

TESTLIST = [TESTDATA1]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_1009(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)



