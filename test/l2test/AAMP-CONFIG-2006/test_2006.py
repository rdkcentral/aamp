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

TESTLIST = list()
lang_dict = {
    "german":{
        "long": "ger",
        "short": "de",
        "track": 0
    },
    "english":{
        "long": "eng",
        "short": "en",
        "track": 2
    },
    "spanish":{
        "long": "spa",
        "short": "es",
        "track": 4
    },
    "french":{
        "long": "fra",
        "short": "fr",
        "track": 6
    },
    "polish":{
        "long": "pol",
        "short": "pl",
        "track": 8
    }
}
for language,details in lang_dict.items():

    TESTLIST.append(
        {
            "title": "Get preferredAudioLanguage",
            "max_test_time_seconds": 15,
            "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
            "expect_list": [
                {"cmd":'setconfig {"info":true,"trace":true}'},
                {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
                {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
                {"cmd": f"set audioTrack {details['track']}"},
                {"expect": r"Matched Command AudioTrack - set audioTrack "+f"{details['track']}"},
                {"expect": r"preferredAudioLanguage New Owner"},
                {"expect": r"preferredAudioLabel New Owner"},
                {"expect": r"preferredAudioRendition New Owner"},
                {"expect": r"preferredAudioType New Owner"},
                {"expect": r"preferredAudioCodec New Owner"},
                # Expected current playback position as a 4 digit number. i.e less than 9999
                {"expect": r"Returning Position as (\d{4}) "},
                {"cmd": "get preferredAudioProperties"},
                {"expect": r"\"preferred\-audio\-languages\":\t\""+f"{details['long']}"+r"\""},
                {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/"+f"{details['short']}"+r"_[0-9]*\.mp3"},
                {"cmd": "sleep 3000"},
                {"expect": "sleeping for 3.000000 seconds"},
                # Playback position is now 5 digit number. confirmed playback progressed.
                {"expect": r"Returning Position as (\d{5}) "},
                
            ]
        })
for language,details in lang_dict.items():
    print(language,details)

    TESTLIST.append(
        {
            "title": "Get preferredAudioLanguage",
            "max_test_time_seconds": 15,
            "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
            "expect_list": [
                {"cmd":'setconfig {"info":true,"trace":true}'},
                {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
                {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
                {"cmd": f"set language {details['long']}"},
                {"expect": r"Matched Command Language - set language "+f"{details['long']}"},
                {"expect": r"preferredAudioLanguage New Owner"},
                # Expected current playback position as a 4 digit number. i.e less than 9999
                {"expect": r"Returning Position as (\d{4}) "},
                {"cmd": "get currentAudioLan"},
                {"expect": r"CURRRENT AUDIO LANGUAGE = "+f"{details['long']}"},
                {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/"+f"{details['short']}"+r"_[0-9]*\.mp3"},
                {"cmd": "sleep 3000"},
                {"expect": "sleeping for 3.000000 seconds"},
                # Playback position is now 5 digit number. confirmed playback progressed.
                {"expect": r"Returning Position as (\d{5}) "},
            ]
        })
for language,details in lang_dict.items():
    print(language,details)

    TESTLIST.append(
        {
            "title": "Get preferredAudioLanguage",
            "max_test_time_seconds": 15,
            "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
            "expect_list": [
                {"cmd":'setconfig {"info":true,"trace":true}'},
                {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
                {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
                {"cmd": f"set preferredLanguages {details['long']}"},
                {"expect": r"Matched Command PreferredLanguages - set preferredLanguages "+f"{details['long']}"},
                {"expect": r"preferredAudioLanguage New Owner"},
                # Expected current playback position as a 4 digit number. i.e less than 9999
                {"expect": r"Returning Position as (\d{4}) "},
                {"cmd": "get currentPreferredLanguages"},
                {"expect": r"PREFERRED LANGUAGES = "+'"'+f"{details['long']}"+'"'},
                {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/"+f"{details['short']}"+r"_[0-9]*\.mp3"},
                {"cmd": "sleep 3000"},
                {"expect": "sleeping for 3.000000 seconds"},
                # Playback position is now 5 digit number. confirmed playback progressed.
                {"expect": r"Returning Position as (\d{5}) "},
            ]
        }
    )
for language,details in lang_dict.items():
    print(language,details)

    details_long = details['long']
    details_short = details['short']
    TESTLIST.append(
        {
            "title": "Get preferredAudioLanguage",
            "max_test_time_seconds": 15,
            "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
            "expect_list": [
                {"cmd":'setconfig {"info":true,"trace":true}'},
                {"cmd":r'setconfig {"preferredAudioLanguage":"'+details_long+'"}'},
                {"cmd":r'setconfig {"preferredAudioLabel":"'+details_long+'"}'},
                {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
                {"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING"},
                # Expected current playback position as a 4 digit number. i.e less than 9999
                {"expect": r"Returning Position as (\d{4}) "},
                {"cmd": "getconfig"},
                {"expect": f'"preferredAudioLanguage":"{details_long}","preferredAudioLabel":"{details_long}"'},
                {"expect": r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/"+f"{details_short}"+r"_[0-9]*\.mp3"},
                {"cmd": "sleep 3000"},
                {"expect": "sleeping for 3.000000 seconds"},
                # Playback position is now 5 digit number. confirmed playback progressed.
                {"expect": r"Returning Position as (\d{5}) "},
            ]
        }
    )

############################################################


@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_2006(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
