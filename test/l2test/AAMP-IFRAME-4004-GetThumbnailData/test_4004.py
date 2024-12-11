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


test_sequence =[]
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

    # Tests to verify success when a valid thumbnail index is provided.
    for idx in range(track_count):
        test_sequence['expect_list'].insert(-1, {"cmd": "set thumbnailTrack {}".format(idx)})
        test_sequence['expect_list'].insert(-1, {"expect": r" SetThumbnailTrack \[{}\] result: success".format(idx)})
        test_sequence['expect_list'].insert(-1, {"cmd": "get thumbnailData 0 120 1"})
        test_sequence['expect_list'].insert(-1, {"expect": r" GETTING THUMBNAIL TIME RANGE DATA (.*)", "callback": ranges_parser})
        test_sequence['expect_list'].insert(-1, {"cmd": "sleep {}".format(sleep_time)})

stream_configuration=[

    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock1/mpeg_2sec/manifest.m3u8"},
    {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/thumbnail_l2/peacock2/mpeg_2sec/manifest.m3u8"},
   {"url":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-IFRAME-4004-GetThumbnailData/spectrum/DASH_DRM/NBCE9000392526190003/ec3/index.ism/manifest.mpd"},

]

def extract_and_append_urls(ranges_info):
    base_url = ranges_info["baseUrl"]
    tile_urls = [tile["url"] for tile in ranges_info["tile"]]
    full_urls = [base_url + tile_url for tile_url in tile_urls]
    return full_urls

def ranges_parser(regex_match):
    '''Checks that the returned JSON object contains at least a tile.
    :param regex_match: the regex returned by pexpect.expect containing the thumbnail data
    :type regex_match: regex
    '''

    ranges_info = json.loads(regex_match.group(1))

    if ranges_info.get('tile') is not None:
        tiles = ranges_info.get('tile')
        current_url = test_sequence['expect_list'][0]['cmd']
        if "AAMP-IFRAME-4004-GetThumbnailData" in current_url:
            full_urls = extract_and_append_urls(ranges_info)
            expected_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-IFRAME-4004-GetThumbnailData/spectrum/DASH_DRM/NBCE9000392526190003/ec3/index.ism/DASH_DRM/NBCE9000392526190003/ec3/index.ism/dash/out_withEac3_v2-img=5000-n-1.jpg"
            # Check if any of the appended URLs match the expected URL
            if expected_url in full_urls:
                print(f"URL matched: {expected_url}")
            else:
                print(f"URL did not match. Got: {full_urls}")
                raise Exception ("URL did not match")
    else:
        raise Exception ("No tiles found in the interval")


@pytest.fixture(params = stream_configuration)
def test_data(request):
    return request.param

def test_4004(aamp_setup_teardown, test_data):
    '''For each of the assets this test: 
    * Plays the asset
    * Extracts the number of thumbanail tracks 
    * Sets each thumbnail track
    * Check that AAMP returns a "success" from the set command if the track index is valid, else it returns "fail"
    * Checks that the thumbnail data contains at least a tile
    '''

    global test_sequence

    single_test_data = {
        "title": "Test get thumbnailData API",
        "max_test_time_seconds": 30,
        "aamp_cfg": "info=true\ntrace=true\n",
        "expect_list": Generate_ExpectList(test_data["url"])
    }

    test_sequence = copy.deepcopy(single_test_data)

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_sequence)
