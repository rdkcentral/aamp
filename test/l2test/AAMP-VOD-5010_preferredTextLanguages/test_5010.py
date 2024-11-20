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
import re
# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/multilingual_subtitles.tar.xz"

TESTDATA1 = {
	"title": "Set preferred Text Languages for french language ",
	"logfile": "testdata1.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages fr"},
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-fr.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-fr-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-fr-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-3](\d{3})"},
		{"expect": r"Returning Position as [4-9](\d{3})"},
		{"cmd": "stop"},
	]
}
 


TESTDATA2 = {
	"title": "Set preferred Text Languages for russian language",
	"logfile": "testdata2.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages ru"},
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-ru.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-3](\d{3})"},
		{"expect": r"Returning Position as [4-9](\d{3})"},
		{"cmd": "stop"},
	]
}


TESTDATA3 = {
	"title": "Set preferred Text Languages changing the subtitle language while video is streaming",
	"logfile": "testdata3.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages ru"},
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-ru.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[2-8].m4s"},
		{"expect": r"Returning Position as [2-4](\d{3})"},
		{"cmd": "sleep 3000"},
		{"expect": "sleep complete"},
		{"cmd":"set preferredTextLanguages en"},
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-en.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-en-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-en-0000[2-8].m4s"},
		{"expect": r"Returning Position as (\d{0,1})[0-4](\d{3})"},
		{"expect": r"Returning Position as (\d{0,1})[5-9](\d{3})"},
		{"cmd": "stop"},
	]
}
 
 
TESTDATA4 = {
	"title": "textTrack to change the subtitles while streaming",
	"logfile": "testdata4.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages de"},
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-de.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-4](\d{3})"},
		{"cmd": "sleep 3000"},
		{"expect": "sleep complete"},
		{"cmd":"set textTrack 3"}, #spanish language 
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-es.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-es-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-es-0000[2-8].m4s"},
		{"expect": r"Returning Position as [6-9](\d{3})"},
		{"expect": r"Returning Position as (\d{0,1})[0-4](\d{3})"},
		{"cmd": "stop"},
	]
}
 

TESTDATA5 = {
	"title": "textTrack to change the subtitles while streaming",
	"logfile": "testdata5.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages pt"}, #portuguese
		{"expect":r"init url http://localhost:8085/multilingual_subtitles/init-stream-subtitle-de.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-4](\d{3})"},
		{"expect": r"Returning Position as [4-9](\d{3})"},
		{"cmd": "stop"},
	]
}

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4, TESTDATA5]



############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_5010(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


