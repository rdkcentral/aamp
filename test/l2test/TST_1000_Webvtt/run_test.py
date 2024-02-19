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

# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

TESTDATA1 = {
"title": "Setting WebVTT font size",
"logfile": "testdata1.txt",
"max_test_time_seconds": 15,
"expect_list": [
    {"cmd":'setconfig {"info":true,"trace":true}'}, # must use " not ' in json
    {"cmd": "set subtecSimulator 1"},
    {"expect": r"SubtecSimulatorThread - listening for packets",},
    {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
    {"expect": r"AAMP_EVENT_TUNED"},
    {"cmd": "set textTrack data/test.vtt"},
    {"expect": r"webvtt data received from application",},
    {"cmd": "set ccStyle data/small.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling SubtitleParser"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 0"},
    {"cmd": "set ccStyle data/medium.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling SubtitleParser"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 1"},
    {"cmd": "set ccStyle data/large.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling SubtitleParser"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 2"},
    {"cmd": "set ccStyle data/extra_large.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling SubtitleParser"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 3"},
    {"cmd": "set ccStyle data/auto.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling SubtitleParser"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 4294967295"},
    {"cmd": "stop"},
    {"cmd": "set subtecSimulator 0"},
    {"expect": r"SubtecSimulatorThread - exit",},
]
}

############################################################


def test_1000(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA1)


