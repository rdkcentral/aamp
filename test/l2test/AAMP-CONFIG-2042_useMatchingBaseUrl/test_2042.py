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
import pytest
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/matchingUrlDump.tar.xz"

TESTDATA1 = {
	"title": "Set useMatchingBaseUrl as true",
	"logfile": "useMatchingBaseUrl_true.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nuseMatchingBaseUrl=true\n",
    "archive_url": archive_url,
	"url":"matchingUrlDump/demo_manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"mManifestUrl: http://localhost:8085/matchingUrlDump/demo_manifest.mpd"},
		{"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
		{"expect":r"Parse MPD Completed ..."},
		{"expect":r"fragmentUrl http://localhost:8085/matchingUrlDump/video_init.mp4"},
		{"expect":r"inserted init_video http://localhost:8085/matchingUrlDump/video_init.mp4"},
		{"expect":r"fragmentUrl http://localhost:8085/matchingUrlDump/audio_init.mp4"},
		{"expect":r"inserted init_audio http://localhost:8085/matchingUrlDump/audio_init.mp4"},
		{"expect":r"Returning Position as [0-4](\d{3})"},
		{"expect":r"Returning Position as [5-9](\d{3})"}, 
	]
}

TESTDATA2 = {
	"title": "Set useMatchingBaseUrl as false",
	"logfile": "useMatchingBaseUrl_false.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nprogress=true\nuseMatchingBaseUrl=false\n",
    "archive_url": archive_url,
	"url":"matchingUrlDump/demo_manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"mManifestUrl: http://localhost:8085/matchingUrlDump/demo_manifest.mpd"},
		{"expect":r"AAMP_EVENT_STATE_CHANGED: INITIALIZING"},
		{"expect":r"Parse MPD Completed ..."},
		{"expect":r"fragmentUrl http://0.0.0.0:8085/video_init.mp4"},
		{"expect":r"Init fragment fetch failed -- fragmentUrl http://0.0.0.0:8085/video_init.mp4"},
		{"expect":r"AAMP_EVENT_TUNE_FAILED reason=AAMP: init fragment download failed : Http Error Code 404"},
		{"expect":r"fragmentUrl http://0.0.0.0:8085/audio_init.mp4"},
		{"expect":r"StreamAbstractionAAMP_MPD: failed. fragmentUrl http://0.0.0.0:8085/audio_init.mp4"},
	]
}

TESTLIST = [TESTDATA1, TESTDATA2]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2042(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
