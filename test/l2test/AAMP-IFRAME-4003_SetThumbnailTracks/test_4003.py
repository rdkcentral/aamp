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
import re
import pytest

import copy
import json

from inspect import getsourcefile

test_sequence = []
sleep_time=5000

def Generate_ExpectList(url):
    
    data = [
        {"cmd": url},
        {"expect": "IP_AAMP_TUNETIME"},
        {"cmd": "sleep {}".format(sleep_time)},
        {"cmd": "get thumbnailConfig"},
        {"expect": r"Current ThumbnailTracks: (.*) \.", "callback": tracks_parser},
        {"cmd": "sleep {}".format(sleep_time)},
    ]
    return data

def tracks_parser(regex_match):
    '''Extracts from the thumbnail configuration the number of tracks
    
    :param regex_match: the regex returned by pexpect.expect containing the thumbnail configuration
    :type regex_match: regex
    '''

    tracks_info = json.loads(regex_match.group(1))
    track_count = len(tracks_info)

    # Test to verify failure when no thumbnail index is provided.
    test_sequence['expect_list'].insert(-1, {"cmd": "sleep {}".format(sleep_time)})
    test_sequence['expect_list'].insert(-1, {"cmd": "set thumbnailTrack"}) # Varifies failure when no thumbnail index is provided.
    test_sequence['expect_list'].insert(-1, {"expect": r" SetThumbnailTrack \[-1\] result: fail"})

    # Tests to verify failure when an invalid thumbnail index is provided.
    randomThumbnailIndexes = [-234255, 456789, 97531, -123456]

    for idx in randomThumbnailIndexes:
        test_sequence['expect_list'].insert(-1, {"cmd": "sleep {}".format(sleep_time)})
        test_sequence['expect_list'].insert(-1, {"cmd": "set thumbnailTrack {}".format(idx)})
        test_sequence['expect_list'].insert(-1, {"expect": r" SetThumbnailTrack \[{}\] result: fail".format(idx)})

    # Tests to verify success when a valid thumbnail index is provided.
    for idx in range(track_count):

        test_sequence['expect_list'].insert(-1, {"cmd": "sleep {}".format(sleep_time)})
        test_sequence['expect_list'].insert(-1, {"cmd": "set thumbnailTrack {}".format(idx)})
        test_sequence['expect_list'].insert(-1, {"expect": r" SetThumbnailTrack \[{}\] result: success".format(idx)})
    
    
stream_configuration=[
    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock1/mpeg_2sec/manifest.m3u8"},
    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock2/mpeg_2sec/manifest.m3u8"},
]

@pytest.fixture(params = stream_configuration)
def test_data(request):
    return request.param

def test_4003(aamp_setup_teardown, test_data):
    '''For each of the assets this test: 
    * Plays the asset
    * Extracts the number of thumbanail tracks 
    * Sets each thumbnail track
    * Check that AAMP returns a "success" from the set command if the track index is valid, else it returns "fail"
    
    This approach makes it easy to extend this test to different assets in the future, 
    provided that the information on the trasks is extracted correctly by the `get thumbnailConfig` command.
    '''

    global test_sequence

    single_test_data = {
        "title": "Test SetThumbnailTrack API",
        "max_test_time_seconds": 60,
        "aamp_cfg": "info=true\ntrace=true\n",
        "expect_list": Generate_ExpectList(test_data["url"])
    }

    test_sequence = copy.deepcopy(single_test_data)

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))

    aamp.run_expect_a(test_sequence)