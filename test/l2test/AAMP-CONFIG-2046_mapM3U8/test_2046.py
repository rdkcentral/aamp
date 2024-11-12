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

TESTDATA1 = {
	# AAMP tries to find m3u8 file having same name as the DASH stream and if m3u8 file is available then it downloads the m3u8 file and playbacks the same.
	"title": "Set mapM3U8 as cpetestutility",
	"logfile": "mapM3U8_cpetestutility.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nmapM3U8=cpetestutility.stb.r53.xcal.tv\n",
	"expect_list": [
		{"cmd":r"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
		# Found .m3u8 file with same name hence downloaded m3u8 file & parsed it.
		{"expect":r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
		# Playlist download request as per ABR
		{"expect":r"Url:https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/(1080|720|480|360)p.m3u8"},
		# Downloaded .ts segments as mentioned in .m3u8 file
		{"expect":r"HttpRequestEnd:([\s0-9., ]+)https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/(1080|720|480|360)p_00[0-2].ts"},
		{"expect":r"HttpRequestEnd:([\s0-9., ]+)https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/hls/(1080|720|480|360)p_00[3-4].ts"},
		# Playback progressing
		{"expect":r"Returning Position as [1-3](\d){3}"},
		{"expect":r"Returning Position as [4-6](\d){3}"},
	]
}
TESTDATA2 = {
	# AAMP tries to find m3u8 file having same name but since the domains are different, it continues the playback of DASH stream passed to it.
	"title": "Set mapM3U8 as localhost",
	"logfile": "mapM3U8_localhost.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nmapM3U8=localhost\n",
	"expect_list": [
		{"cmd":r"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
		# Same DASH is downloaded and parsed
		{"expect":r"mManifestUrl: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
		# Downloaded init segment as mentioned in .mpd file
		{"expect":r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(1080|720|480|360)p_init.m4s"},
		# Downloaded .m4s segments as mentioned in .mpd file
		{"expect":r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(1080|720|480|360)p_00[1-2].m4s"},
		{"expect":r"fragmentUrl https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/(1080|720|480|360)p_00[3-4].m4s"},
		# Playback progressing
		{"expect":r"Returning Position as [1-3](\d){3}"},
		{"expect":r"Returning Position as [4-6](\d){3}"},
	]
}

TESTLIST = [TESTDATA1, TESTDATA2]


############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
	return request.param

def test_2046(aamp_setup_teardown, test_data):
	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_a(test_data)
