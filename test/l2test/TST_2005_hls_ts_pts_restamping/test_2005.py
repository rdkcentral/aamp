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

# Starts simlinear, a web server for serving ABR test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
#
# Also see README.txt

from inspect import getsourcefile
import os
import pytest
import re

###############################################################################

# this archive includes both vod and live manifests pointing to same harvested content
# note: only lowest profile segments are available
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/2005/xumo-cc.zip"

TESTDATA1 = {
"title": "multiperiod muxed hls/ts playback with webvtt track",
"archive_url": archive_url,
# not yet able to get this live hls content playing through simlinear
# "url": "xumo-cc/d2it737nair3v7.cloudfront.net/30003/88889579/hls/manifest-simlinear.m3u8",
"url": "xumo-cc/d2it737nair3v7.cloudfront.net/30003/88889579/hls/manifest-vod.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 180,
# for vod, play starting 330s into stream (close to first period end)
"aamp_cfg": "info=true\nenablePTSRestampForHlsTs=true\noffset=330\n",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    # TODO: can we get webvtt subtitle track also playing/validated in l2 test?
    {"expect": re.escape("pts_offset[0]=-1379368ms") },
    {"expect": re.escape("pts_offset[1]=24192ms") },
    {"expect": re.escape("pts_offset[2]=27962ms") },
    {"expect": re.escape("pts_offset[3]=148049ms") },
    {"expect": re.escape("pts_offset[4]=-1251783ms") },
    {"expect": re.escape("pts_offset[5]=862899ms") },
    {"expect": re.escape("pts_offset[6]=868071ms"), "end_of_test":True }
    ]
}

archive_url2 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/TST_2005_HLS_TS_PTS_RESTAMPING/d32e20a7-b227-4574-885b-4b9da697110b.zip"

TESTDATA2 = {
"title": "muxed hls/ts playback large pts",
"archive_url": archive_url2,
"url": "d32e20a7-b227-4574-885b-4b9da697110b/vod_4.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 10,
"aamp_cfg": "info=true\nenablePTSRestampForHlsTs=true\n",
"expect_list": [
	{"expect": r"aamp_tune"},
    {"expect": r"PAUSED -> PLAYING"},
    {"expect": r"NotifyFirstFrameReceived"},
    {"expect": r"SendTuneMetricsEvent","end_of_test":True}
    ]
}

#The full list of tests
TESTLIST = [TESTDATA1, TESTDATA2]

############################################################

"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param


def test_2000(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    print(test_data['title'])
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

