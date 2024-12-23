#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024  RDK Management
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

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/multiIframeTracks.tar.xz"

TESTDATA1 = {
	"title": "Set iframeDefaultBitrate as default",
	"logfile": "testdata_default.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true",
    "archive_url": archive_url,
	"url":f"manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
		{"expect":r"Returning Position as [3-6](\d{3})"},
		{"cmd":r"ff 4"},
		{"expect":r"aamp_SetRate rate\(1\.000000\)->\(4\.000000\)"},
		{"expect":r"fragmentUrl http://localhost:8085/init-stream[3-5].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream[3-5]-0000[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream[3-5]-0001[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream[3-5]-0002[0-9].m4s"},
		{"cmd":r"sleep 3000"},
		{"expect":r"sleep complete"},
		{"cmd":r"play"},
		{"expect":r"aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
		{"expect":r"Returning Position as 1[4-9](\d{3})"},
		{"expect":r"Returning Position as 2[0-5](\d{3})"},
	]
}
TESTDATA2 = {
	"title": "Set iframeDefaultBitrate as 360p iframe track",
	"logfile": "testdata_360p.txt",
	"max_test_time_seconds": 15,
	# Bitrate specified in the manifest for AdaptationSet id 3 (640x320p)
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\niframeDefaultBitrate=148511\n",
    "archive_url": archive_url,
	"url":f"manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
		{"expect":r"Returning Position as [3-6](\d{3})"},
		{"cmd":r"ff 4"},
		{"expect":r"aamp_SetRate rate\(1\.000000\)->\(4\.000000\)"},
		{"expect":r"fragmentUrl http://localhost:8085/init-stream3.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream3-0000[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream3-0001[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream3-0002[0-9].m4s"},
		{"cmd":r"sleep 3000"},
		{"expect":r"sleep complete"},
		{"cmd":r"play"},
		{"expect":r"aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
		{"expect":r"Returning Position as 1[4-9](\d{3})"},
		{"expect":r"Returning Position as 2[0-5](\d{3})"},
	]
}
TESTDATA3 = {
	"title": "Set iframeDefaultBitrate as 720p iframe track",
	"logfile": "testdata_720p.txt",
	"max_test_time_seconds": 15,
	# Bitrate specified in the manifest for AdaptationSet id 4 (1280x720p)
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\niframeDefaultBitrate=315800\n",
    "archive_url": archive_url,
	"url":f"manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
		{"expect":r"Returning Position as [3-6](\d{3})"},
		{"cmd":r"ff 4"},
		{"expect":r"aamp_SetRate rate\(1\.000000\)->\(4\.000000\)"},
		{"expect":r"fragmentUrl http://localhost:8085/init-stream4.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream4-0000[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream4-0001[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream4-0002[0-9].m4s"},
		{"cmd":r"sleep 3000"},
		{"expect":r"sleep complete"},
		{"cmd":r"play"},
		{"expect":r"aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
		{"expect":r"Returning Position as 1[4-9](\d{3})"},
		{"expect":r"Returning Position as 2[0-5](\d{3})"},
	]
}
TESTDATA4 = {
	"title": "Set iframeDefaultBitrate as 4k iframe track",
	"logfile": "testdata_4k.txt",
	"max_test_time_seconds": 15,
	# Bitrate specified in the manifest for AdaptationSet id 5 (3840x2160p)
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\niframeDefaultBitrate=1073500\n",
    "archive_url": archive_url,
	"url":f"manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"expect":r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
		{"expect":r"Returning Position as [3-6](\d{3})"},
		{"cmd":r"ff 4"},
		{"expect":r"aamp_SetRate rate\(1\.000000\)->\(4\.000000\)"},
		{"expect":r"fragmentUrl http://localhost:8085/init-stream5.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream5-0000[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream5-0001[0-9].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/chunk-stream5-0002[0-9].m4s"},
		{"cmd":r"sleep 3000"},
		{"expect":r"sleep complete"},
		{"cmd":r"play"},
		{"expect":r"aamp_SetRate rate\(4\.000000\)->\(1\.000000\)"},
		{"expect":r"Returning Position as 1[4-9](\d{3})"},
		{"expect":r"Returning Position as 2[0-5](\d{3})"},
	]
}
TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4]
############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param
def test_2039(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
