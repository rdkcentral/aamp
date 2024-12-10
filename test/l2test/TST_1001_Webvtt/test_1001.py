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
# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

# This test is sending font size command to subtec (using subtec simulator)
# via Gstreamer subtecbin plugin.
TESTDATA1 = {
"title": "Setting WebVTT font size AAMP - GStreamer - Subtec",
"max_test_time_seconds":60,
"aamp_cfg": "info=true\ntrace=true\ngstSubtecEnabled=true\n",
"expect_list": [
    {"cmd": "set subtecSimulator 1"},
    {"expect": r"SubtecSimulatorThread - listening for packets",},
    {"cmd": "https://storage.googleapis.com/shaka-demo-assets/angel-one-hls/hls.m3u8"},
    {"expect": r"AAMPGstPlayer_SetupStream - subs using subtecbin"},
    {"expect": r"AAMP_EVENT_TUNED"},
    {"cmd": "set 43 0"},
    {"expect": r"Type:WEBVTT_SELECTION"},  # Subtec simulator has been configured for subtitles, now change size
    {"cmd": "set ccStyle data/small.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling StreamSink::SetTextStyle"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 0"},
    {"cmd": "set ccStyle data/medium.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling StreamSink::SetTextStyle"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 1"},
    {"cmd": "set ccStyle data/large.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling StreamSink::SetTextStyle"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 2"},
    {"cmd": "set ccStyle data/extra_large.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling StreamSink::SetTextStyle"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 3"},
    {"cmd": "set ccStyle data/auto.json"},
    {"expect": r"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect": r"Calling StreamSink::SetTextStyle"},
    {"expect": r"\*\*\*SubtecSimulatorThread:"},
    {"expect": r"Type:CC_SET_ATTRIBUTE"},
    {"expect": r"attribute\[5\]: 4294967295"},
    {"cmd": "stop"},
    {"cmd": "set subtecSimulator 0"},
    {"expect": r"SubtecSimulatorThread - exit"},

]
}

############################################################

# Cannot get to run on mac
# ==60199==ERROR: Interceptors are not working. This may be because AddressSanitizer is loaded too late


@pytest.mark.ci_test_set
@pytest.mark.skipif(platform.system() == 'Darwin', reason="Not working on mac")
def test_1001(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))

    lib_path = os.path.join(aamp.aamp_home, ".libs", "lib", "gstreamer-1.0")

    aamp.AAMP_ENV.update({"GST_PLUGIN_PATH": lib_path})

    assert os.path.exists(lib_path), "Missing subtec plugin, did it get built? {}".format(lib_path)
    aamp.run_expect_a(TESTDATA1)
