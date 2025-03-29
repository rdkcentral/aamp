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


HLS_URL     = "https://bitdash-a.akamaihd.net/content/sintel/hls/playlist.m3u8"

######### DASH TEST CADSES ############################

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
        # AAMP config languageCodePreference is the default 0 (=ISO639_NO_LANGCODE_PREFERENCE),
		# so codes will *NOT* be normalised to 2- or 3-digit variants.
		# The client must therefore pass a language code that matches the code in the manifest
		# to the preferredTextLanguages API.
		{"cmd":"set preferredTextLanguages fr"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-fr.m4s"},
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
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\nlangCodePreference=2\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
        # The manifest has "ru", but as languageCodePreference is 2 (=ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE),
		# codes will be normalised to 3-digit terminology variants.
		# To verify this, pass in a 3-digit terminology code via the API.
		{"cmd":"set preferredTextLanguages rus"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-ru.m4s"},
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
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\nlangCodePreference=3\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":"set preferredTextLanguages ru"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-ru.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-ru-0000[2-8].m4s"},
		{"expect": r"Returning Position as [2-4](\d{3})"},
		{"cmd": "sleep 3000"},
		{"expect": "sleep complete"},
        # The manifest has "en", but as languageCodePreference is 3 (=ISO639_PREFER_2_CHAR_LANGCODE),
		# codes will be normalised to 2-digit variants.
		# To verify this, pass in a 3-digit code via the API.
		{"cmd":"set preferredTextLanguages eng"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-en.m4s"},
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
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\nlangCodePreference=1\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
        # The manifest has "de", but as languageCodePreference is 1 (=ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE),
		# codes will be normalised to 3-digit bibliographic variants.
		# To verify this, pass in a 3-digit bibliographic code via the API.
		{"cmd":"set preferredTextLanguages ger"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-de.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-4](\d{3})"},
		{"cmd": "sleep 3000"},
		{"expect": "sleep complete"},
		{"cmd":"set textTrack 3"}, #spanish language
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-es.m4s"},
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
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-de.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-de-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-4](\d{3})"},
		{"expect": r"Returning Position as [4-9](\d{3})"},
		{"cmd": "stop"},
	]
}

#Test comma-delimited api
TESTDATA6 = {
	"title": "Set preferred Text Languages for french language ",
	"logfile": "testdata6.txt",
	"max_test_time_seconds": 25,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\ninitialBitrate=401000\n",
    "archive_url": archive_url,
	"url":"multilingual_subtitles/manifest.mpd",
	"simlinear_type": "DASH",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
        # AAMP config languageCodePreference is the default 0 (=ISO639_NO_LANGCODE_PREFERENCE),
		# so codes will *NOT* be normalised to 2- or 3-digit variants.
		# The client must therefore pass a language code that matches the code in the manifest
		# to the preferredTextLanguages API.  Multiple codes can be passed for the same language
		# providing they include the code used in the manifest, but note that this will cause a
		# retune if streaming has already started.  If the client is using ISO639_NO_LANGCODE_PREFERENCE,
        # then it is responsible for ensuring that the codes passed to the API match the codes in the manifest.
		{"cmd":"set preferredTextLanguages fra,fr,fre"},
		{"expect":r"inserted init_text http://localhost:8085/multilingual_subtitles/init-stream-subtitle-fr.m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-fr-0000[1-3].m4s"},
		{"expect":r"fragmentUrl http://localhost:8085/multilingual_subtitles/chunk-stream-subtitle-fr-0000[2-8].m4s"},
		{"expect": r"Returning Position as [1-3](\d{3})"},
		{"expect": r"Returning Position as [4-9](\d{3})"},
		{"cmd": "stop"},
	]
}

######### HLS TEST CADSES ############################

#Test to check the default subtitle is selected and the textTrack is selected to 5.
#The test case is to validate the scenario identifeid in DELIA-66656
TESTDATA7 = {
	"title": "Ensuring that the default textTrack is selected",
	"logfile": "testdata7.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\n",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":HLS_URL},
        {"cmd": "sleep 3000"},
		{"expect": r"TextTrack Selected :5"},
		{"cmd": "stop"},
	]
}


#Test to check the preferred subtitle langhuage is set to de and the textTrack is selected to 4.
#The test is to ensure clinet can modify the preferred subtitle language as required
TESTDATA8 = {
	"title": "Set preferred Text Languages for french language ",
	"logfile": "testdata8.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\npreferredSubtitleLanguage=de\n",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
        {"cmd":"set preferredTextLanguages de"},
		{"cmd":HLS_URL},
        {"cmd": "sleep 3000"},
		{"expect": r"TextTrack Selected :4"},
		{"cmd": "stop"},
	]
}


#Test comma-delimited api to set the preferred subtitle language to en and eng
#The test case is to validate the scenario identifeid in DELIA-66656
TESTDATA9 = {
	"title": "Ensuring that the default textTrack is selected",
	"logfile": "testdata9.txt",
	"max_test_time_seconds": 15,
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nforceHttp=true\npreferredSubtitleLanguage=en,eng\n",
	"expect_list": [
		{"cmd":"set subtecSimulator 1"},
		{"cmd":HLS_URL},
        {"cmd": "sleep 3000"},
		{"expect": r"TextTrack Selected :5"},
		{"cmd": "stop"},
	]
}

TESTDATA10 = {
        "title": "Confirm language selection using SetPreferredTextLanguages",
        "logfile": "testdata10.txt",
        "max_test_time_seconds": 30,
        "aamp_cfg": f"info=true\nforceHttp=true\n",
        "expect_list": [
                {"cmd":"set subtecSimulator 1"},
                {"cmd":HLS_URL},
        {"cmd": "sleep 3000"},
                {"expect": r"TextTrack Selected :5"},
                {"cmd":"set 33 de"},
                {"expect": r"Preferred Text languages string: de"},
                {"expect": r"AAMP_EVENT_TEXT_TRACKS_CHANGED"},
        {"cmd": "sleep 3000"},
                {"cmd" :"get 27"},
                {"expect": r" CURRENT TEXT TRACK INFO: \[\{" },
                {"expect": "\"name\":\t\"Deutsch\","},
                {"expect": "\"language\":\t\"de\","},
                {"expect": "\"rendition\":\t\"subs\""},
                {"expect": r"\}\]"},
                {"cmd": "stop"},
                {"cmd": "set subtecSimulator 0"},
        ]
}

TESTDATA11 = {
        "title": "Confirm multiple language selection using SetPreferredTextLanguages",
        "logfile": "testdata11.txt",
        "max_test_time_seconds": 30,
        "aamp_cfg": f"info=true\nforceHttp=true\n",
        "expect_list": [
                {"cmd":"set subtecSimulator 1"},
                {"cmd":HLS_URL},
         {"cmd": "sleep 3000"},
                {"cmd":"set 33 es"},
                {"expect": r"Preferred Text languages string: es"},
                {"expect": r"AAMP_EVENT_TEXT_TRACKS_CHANGED"},
                {"cmd" :"get 27"},
                {"expect": r" CURRENT TEXT TRACK INFO: \[\{" },
                {"expect": "\"name\":\t\"Espanol\","},
                {"expect": "\"language\":\t\"es\","},
                {"expect": "\"rendition\":\t\"subs\""},
                {"expect": r"\}\]"},
        {"cmd": "sleep 3000"},
                {"cmd":"set 33 fr"},
                {"expect": r"Preferred Text languages string: fr"},
                {"cmd" :"get 27"},
                {"expect": r" CURRENT TEXT TRACK INFO: \[\{" },
                {"expect": "\"name\":\t\"Français\","},
                {"expect": "\"language\":\t\"fr\","},
                {"expect": "\"rendition\":\t\"subs\""},
                {"expect": r"\}\]"},
                {"cmd": "stop"},
                {"cmd": "set subtecSimulator 0"},
        ]
}

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3, TESTDATA4, TESTDATA5,TESTDATA6,TESTDATA7,TESTDATA8,TESTDATA9,TESTDATA10,TESTDATA11]



############################################################
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_5010(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
