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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/discontinuityTestStream.tar.xz"

TESTDATA0 = {
	"title": "Set mpdDiscontinuityHandling as false",
	"logfile": "mpdDiscontinuityHandling_false.txt",
	"max_test_time_seconds": 35,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nmpdDiscontinuityHandling=false\n",
    "archive_url": archive_url,
	"url":f"discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd",
	"simlinear_type": "DASH",
    "cmdlist": [
        "seek 58"
    ],
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING", "min": 0, "max": 3},
		{"expect":r"Parse MPD Completed ...", "min": 0, "max": 3},
		# Because of discountinuity, GST Errors are logged.
		{"expect":r"GST_MESSAGE_ERROR source: Internal data stream error.", "min": 0, "max": 3},
		{"expect":r"Schedule retune for GstPipeline Error", "min": 0, "max": 3},
		{"expect":r"Anomaly evt:1 msg:video GstPipeline Internal Error", "min": 0, "max": 3},
		{"expect":r"PrivateInstanceAAMP: Schedule Retune errorType 6 error GstPipeline Internal Error", "min": 0, "max": 3},
		{"expect":r"streaming stopped, reason not-negotiated", "min": 0, "max": 3},
		{"expect":r"Destroying gstreamer pipeline", "min": 0, "max": 3},
		{"expect":r"exiting AAMPGstPlayer_Stop", "min": 0, "max": 3},
		# Because of discountinuity handling is disabled, AAMP will not detect discontinuities "StreamAbstractionAAMP_MPD: discontinuity detected".
		{"expect":r"StreamAbstractionAAMP_MPD: discontinuity detected", "min": 0, "max": 10, "not_expected" : True},
		{"expect":r"Returning Position as 5[7-9](\d{3})", "min": 11, "max": 15, "end_of_test": True},
	]
}

TESTDATA1 = {
	"title": "Set mpdDiscontinuityHandling as true",
	"logfile": "mpdDiscontinuityHandling_true.txt",
	"max_test_time_seconds": 35,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nmpdDiscontinuityHandling=true\n",
    "archive_url": archive_url,
	"url":f"discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
		{"expect":r"Parse MPD Completed ..."},
		{"expect":r"StreamAbstractionAAMP_MPD: discontinuity detected"},
		{"expect":r"Current fragstarttime : 0.000000 nextfragmentTime : 0.000000"},
		{"expect":r"0.000000 discontinuity 1 pto 0.000000 scale 0 duration 0.000000 mPTSOffsetSec 0.000000 absTime 60.000000 fragmentUrl http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/../../Content/video/h264/[1-2]000k/2second/tears_of_steel_1080p_[1-2]000k_h26[4-5]_dash_track1_init.mp4"},
		{"expect":r"Current fragstarttime : 0.000000 nextfragmentTime : 0.000000"},
		{"expect":r"0.000000 discontinuity 1 pto 0.000000 scale 0 duration 0.000000 mPTSOffsetSec 0.000000 absTime 60.000000 fragmentUrl http://localhost:8085/discontinuityTestStream/dash.akamaized.net/dash264/TestCasesIOP33/multiplePeriods/2/../../Content/audio/mp4a/2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4"},
		{"cmd":"seek 58"},
		{"expect":r"track video - encountered aamp discontinuity @position - 0.000000, isDiscoIgnoredForOtherTrack - 0"},
		{"expect":r"Going into wait for processing discontinuity in other track!"},
		{"expect":r"Entering InterfacePlayerRDK: type\(0\) format\(2\) firstBufferProcessed\(1\)"},
		{"expect":r"Returning Position as 6[0-4](\d{3})"},
		{"expect":r"Returning Position as 6[5-9](\d{3})"},
		{"expect":r"Returning Position as 7[0-4](\d{3})"},
	]
}


############################################################

def test_2041_0(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA0)

def test_2041_1(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA1)
