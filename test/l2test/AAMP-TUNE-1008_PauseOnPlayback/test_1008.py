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

from inspect import getsourcefile
import os
import pytest

TESTDATA1= {
"title": "Pause On Playback",
"max_test_time_seconds": 10,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
   {"cmd": "progress"},
   {"cmd": "autoplay"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PREPARED"},
   {"cmd": "pause"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PLAYING"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PAUSED"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_SPEED_CHANGED current rate=0.000000"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_PROGRESS"},
   {"expect": r"\tposition=0.000s"},
   {"expect": r"\tcurrRate=0.0"},
   {"expect": r"\tcurrentPlayRate=0.0"},
   {"cmd": "sleep 1000"},
   {"expect": r"sleep complete"},

   # Check playback works as expected
   {"cmd": "play"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PLAYING"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_SPEED_CHANGED current rate=1.000000"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_PROGRESS"},
   {"expect": r"\tcurrRate=1.0"},
   {"expect": r"\tcurrentPlayRate=1.0"},
   {"cmd": "sleep 1000"},
   {"expect": r"sleep complete"},
   {"cmd": "stop"},

   # Check optional seek sets the position before doing the pause
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PREPARED"},
   {"cmd": "seek 10"},
   {"cmd": "pause"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PLAYING"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PAUSED"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_SPEED_CHANGED current rate=0.000000"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_PROGRESS"},
   {"expect": r"\tposition=10.000s"},
   {"expect": r"\tcurrRate=0.0"},
   {"expect": r"\tcurrentPlayRate=0.0"},
   {"cmd": "sleep 1000"},
   {"expect": r"sleep complete"},

   # Check playback works as expected
   {"cmd": "play"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_STATE_CHANGED: PLAYING"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_SPEED_CHANGED current rate=1.000000"},
   {"expect": r"\[AAMPCLI\] AAMP_EVENT_PROGRESS"},
   {"expect": r"\tcurrRate=1.0"},
   {"expect": r"\tcurrentPlayRate=1.0"},
   {"cmd": "sleep 1000"},
   {"expect": r"sleep complete"},
   {"cmd": "stop"},
]
}
############################################################

@pytest.mark.ci_test_set
def test_1008(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA1)

