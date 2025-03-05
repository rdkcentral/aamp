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
import pytest
import re

from inspect import getsourcefile

archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/aamptest/streams/simlinear/SkyWitness/30t-after-fix/skywitness-30t-after-fix.zip"
archive_url1 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-AUDIO-13004/HLS-LoveThePlanet.zip"

TESTDATA1 = {
     "title": f"Test multi audio profile",
     "max_test_time_seconds": 90,
     "aamp_cfg": f"info=true\ntrace=true\nabr=false\n",
     "expect_list": [
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 2"},
        {"expect": "Matched Command AudioTrack - set 42 2"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"eng\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},

	{"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 0"},
        {"expect": "Matched Command AudioTrack - set 42 0"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"ger\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},

	{"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 4"},
        {"expect": "Matched Command AudioTrack - set 42 4"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"spa\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},

	{"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 6"},
        {"expect": "Matched Command AudioTrack - set 42 6"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"fra\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},

	{"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "set 42 8"},
        {"expect": "Matched Command AudioTrack - set 42 8"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"pol\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"bandwidth\":\t288000,"},
        {"expect": "\"type\":\t\"audio\""},
    ]
}

TESTDATA2 = {
     "title": f"test hls muxed audio",
     "max_test_time_seconds": 30,
     "aamp_cfg": f"info=true\nprogress=true\nenablePublishingMuxedAudio=true\n",
     "expect_list": [

	{"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/muxed/main_mux.m3u8"},
	{"expect": r"aamp_tune", "max":1},
	{"cmd": "sleep 4000"},
        {"expect": "sleeping for 4.000000 seconds"},
        {"cmd":"get 20"},
        {"expect": "\"language\":\t\"eng\","},
        {"cmd": "set 42 2"},
        {"expect": "Parsed preferred lang: deu"},
	{"cmd": "sleep 4000"},
        {"expect": "sleeping for 4.000000 seconds"},
        {"cmd": "get 26"},
        {"expect": "\"language\":\t\"deu\","},
        {"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"characteristics\":\t\"muxed-audio\""},
     ]
}

TESTDATA3 = {
    "title": "Test case to validate availableAudioTracks",
    "max_test_time_seconds": 30,
    "archive_url": archive_url,
    "url":"v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
    "simlinear_type": "DASH",
    "aamp_cfg": f"info=true\nprogress=true\n",
    "expect_list": [
        {"expect": re.escape("Successfully parsed Manifest ...IsLive[1]")},
        {"cmd":"get 20"},
	{"expect": r"AVAILABLE AUDIO TRACKS"},
        {"expect": "\"name\":\t\"root_audio111\","},
        {"expect": "\"language\":\t\"en\","},
        {"expect": "\"codec\":\t\"ec-3\","},
        {"expect": "\"rendition\":\t\"alternate\","},
        {"expect": "\"accessibilityType\":\t\"description\","},
        {"expect": "\"bandwidth\":\t117600,"},
        {"expect": "\"Type\":\t\"audio_description\","},
        {"expect": "\"default\":\tfalse,"},
	{"expect": "\"availability\":\ttrue,"},
	{"expect": "\"accessibility\":\t{"},
	{"expect": "\"scheme\":\t\"urn:mpeg:dash:role:2011\","},
	{"expect": "\"string_value\":\t\"description\""},
    ]
}

TESTDATA4 = {
    "title": "Test case to validate availableAudioTracks with specific codec",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\nprogress=true\nenablePublishingMuxedAudio=true\n",
    "archive_url": archive_url1,
    "url": "11802/88889518/hls/master-cc.m3u8",
    "simlinear_type": "HLS",
    "expect_list": [
	{"expect": r"found audio#0 in program 1 with pcr pid 256 audio pid 257 lan: codec:mp4a.40.2 group:"},
        {"not_expect": r"found audio#1 in program 1 with pcr pid 256 audio pid 500 lan: codec:UNKNOWN group:"},
        {"cmd": "get 20"},
        {"expect": r"AVAILABLE AUDIO TRACKS"},
	{"expect": "\"codec\":\t\"mp4a.40.2\","},
        {"expect": "\"characteristics\":\t\"muxed-audio\""},
        {"not_expect": "\"codec\":\t\"UNKNOWN"},
    ]
}

TESTDATA = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]
@pytest.fixture(params=TESTDATA)
def test_data(request):
    return request.param

# Get all audio tracks : get 20
# Set audio track : set 42 <track_id> 


def test_13004(aamp_setup_teardown,test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)



