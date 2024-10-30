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

# Plays through a list of different manifests
# For each manifest verify that after restamping of segments then the pts
# written to those segments goes like
# seg    PTS value
#  1      0
#  2      pts(1) + seg_duration(1)
#  3      pts(2) + seg_duration(2)
#  n      pts(n-1) + seg_duration(n-1)
# etc
# Also see README.md

import os
from inspect import getsourcefile
import pytest
from l2test_pts_restamp import PtsRestampUtils

manifest_list = [
    {'url': "https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd", 'expected_restamps': 30 },
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/stitched/manifest.mpd", 'expected_restamps': 40 },
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main-multi.mpd", 'expected_restamps': 40 },
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd", 'expected_restamps': 40},
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/misc/main-segmentlist.mpd", 'expected_restamps': 40},
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/misc/main-segmentbase.mpd", 'expected_restamps': 40},
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main-multi.mpd", 'expected_restamps': 40},
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/RDKAAMP-1540/4k/h265/SegmentTimeline.mpd", 'expected_restamps': 40},
    {'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/Sports-Variations/Good_Bad/manifest_x4_good_bad.mpd", 'expected_restamps': 120}
]
###############################################################################
timeout = 120
pts_restamp_utils = PtsRestampUtils()

TESTDATA0 = {
"title": "Verify continuous PTS restamp on various streams",
"max_test_time_seconds": timeout,
"url": "Various streams",
"aamp_cfg": "info=true\nenablePTSReStamp=true\nprogress=true\n",

"expect_list": [
    {"expect": r"aamp_tune","min":0, "max":2},
    {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-]+)\r\n',"min":0, "max":timeout, "callback" : pts_restamp_utils.check_restamp},
    {"expect": r'AAMPGstPlayer_EndOfStreamReached',"min":0, "max":timeout, "end_of_test": True},
    {"expect": r'AAMPGstPlayer_Stop', "min": 0, "max": timeout, "end_of_test": True},
    {"expect": r'StopInternal', "min": 0, "max": timeout, "end_of_test": True},
    ]
}
@pytest.fixture(params=manifest_list)
def test_data(request):
    return request.param

def test_8003_0(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    def segment_cnt_reached():
        aamp.sendline("stop")

    TESTDATA0['url'] = test_data['url']
    TESTDATA0['title'] = "Verify continuous PTS restamp on " + test_data['url']

    pts_restamp_utils.reset()
    pts_restamp_utils.max_segment_cnt = test_data['expected_restamps']
    pts_restamp_utils.segment_cnt_reached = segment_cnt_reached

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(TESTDATA0)

    pts_restamp_utils.check_num_segments()
