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

# @details The test cases include audio track selection, codec switching, and subtitle handling for dash multiview.

import os
import pytest
import time
from inspect import getsourcefile

# Required test streams

URL = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"
URL2 = "https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd"
URL3 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/manifest.mpd"

## @brief Test case for seamless audio switching during playback.
#  @note This test verifies seamless switching between audio tracks.

TESTDATA1 = {
	"title": "Seamless Audio Switch Test",
	"logfile": "AudioSwitch.log",
	"max_test_time_seconds":30,
	"aamp_cfg": "debug=true\nprogress=true\ninfo=true\ntrace=true\nseamlessAudioSwitch=true\nenableMediaProcessor=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"cmd": URL},
		{"expect": r"aamp_tune"},
		{"expect": r"Successfully parsed Manifest"},
		{"expect": "GetBestAudioTrackByLanguage"},
		{"expect": r"Queueing content protection from StreamSelection"},
		{"expect": r"Selected Audio Track: Index:4-0"},
		{"expect": r"\[sendSegment\]\[\d+\]IsoBmffProcessor audio sending segment at pos:0.000000 dur:0.000000"},
		{"expect": r"\[RestampPts\]\[\d+\].*?before 0 after 0 duration \d+ https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/dash/480p_001.m4s"},
		{"expect": "IP_AAMP_TUNETIME"},
		{"not_expect": "Unable to get audioAdaptationSet"},
		{"cmd": "set 32 german"},
		{"cmd": "sleep 5000"},
		{"expect": r"Parsed preferred lang: german"},
		{"cmd": "set 32 english"},
		{"expect": r"Parsed preferred lang: english"},
		{"cmd": "set 42 6"},
		{"cmd": "sleep 2000"},
		{"expect": r"Parsed preferred lang: fra"},
		{"expect": r"PreferredCodecString mp4a.40.2"},
		{"expect": r"Seamless audio switch has been enabled"},
		{"expect": r"FlushTrack()"},
		{"cmd": "exit"},
    ]
}

## @brief Test case for codec switching during playback.
#  @note This test verifies switching between different audio codecs.

TESTDATA2 = {
	"title": "Audio Codec Switching Test",
	"logfile": "CodecSwitch.log",
	"max_test_time_seconds":30,
	"aamp_cfg": "debug=true\nprogress=true\ninfo=true\ntrace=true\nseamlessAudioSwitch=true\nenableMediaProcessor=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"cmd": URL2},
		{"cmd": "set 32 en alternate audio mp4a.40.5"},
		{"expect": r"AudioType Changed 1 -> 3"},
		{"expect": r"Parsed preferred lang: en"},
		{"expect": r"Parsed preferred codec: mp4a.40.5"},
		{"expect": r"Selected Audio Track: Index:4-0 language:en rendition:alternate name:root_audio131 label: type:audio_description codec:mp4a.40.5"},
		{"cmd": "set 32 en main audio ec-3"},
		{"expect": r"Parsed preferred lang: en"},
		{"expect": r"Parsed preferred codec: ec-3"},
		{"expect": r"AudioType Changed 2 -> 3"},
		{"not_expect": r"Seamless audio switch has been enabled"},
		{"not_expect": r"FlushTrack()"},
		{"cmd": "exit"},
    ]
}

## @brief Test case for subtitle handling during playback.
#  @note This test verifies the proper selection of subtitles.

TESTDATA3 = {
	"title": "Subtitle Track Selection Test",
	"logfile": "SubtitleSwitch.log",
	"max_test_time_seconds":30,
	"aamp_cfg": "debug=true\nprogress=true\ninfo=true\ntrace=true\nseamlessAudioSwitch=true\nenableMediaProcessor=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
	{"cmd":"set subtecSimulator 1"},
	{"cmd": URL3},
	{"expect": r"Selected first subtitle track, lang:eng, index:12-0"},
	{"expect": r"Media\[text]\ enabled"},
	{"expect":r"Selected Text Track: Index:12-0 language:eng rendition:caption name:English TTML captions label: type:subtitle codec:stpp isCC:0 Accessibility:NULL"},
	{"expect": r"fragment injector started. track text"},
	{"cmd": "sleep 5000"},
	{"cmd": "set 43 1"},
	{"expect": r"GetPreferredTextTrack 0 trackId 1"},
	{"expect": r"Seamless Text switch has been enabled"},
	{"expect": r"AAMP_EVENT_TEXT_TRACKS_CHANGED"},
	{"expect": r"FlushTrack()"},
	{"cmd": "exit"},
    ]
}

TESTDATA4 = {
   "title": "Audio Switch Test",
   "logfile": "AudioSwitch2.log",
   "max_test_time_seconds":10,
   "aamp_cfg": "suppressDecode=true",
   "expect_list":
    [
          {"cmd": URL},
          {"expect": r"aamp_tune"},
          {"expect": r"Successfully parsed Manifest"},
          {"expect": "GetBestAudioTrackByLanguage"},
          {"expect": r"Queueing content protection from StreamSelection"},
          {"expect":"Selected Audio Track: Index:4-0"},
          {"cmd": "set 42 {\"language\":\"fra\",\"codec\":\"mp4a.40.2\",\"rendition\":\"french\",\"bandwidth\":288000,\"Type\":\"audio\",\"availability\":true}"},
          {"expect": r"Matched Command AudioTrack - set 42 {\"language\":\"fra\",\"codec\":\"mp4a.40.2\",\"rendition\":\"french\",\"bandwidth\":288000,\"Type\":\"audio\",\"availability\":true}"},
          {"cmd": "sleep 2000"},
          {"expect":"Selected Audio Track: Index:8-0"},
    ]
}

TESTDATA = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4]

@pytest.fixture(params=TESTDATA)
def test_data(request):
    return request.param

def test_5000(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
