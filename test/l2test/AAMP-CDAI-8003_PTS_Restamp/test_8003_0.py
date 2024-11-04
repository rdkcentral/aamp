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
import time
import re
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
restamp_values:dict[str, float] = {}
segment_cnt = 0
max_segment_cnt = 0
aamp = None
timeout = 120

def check_restamp(match,arg):
    global segment_cnt, restamp_values, max_segment_cnt

    # Get the fields from the log line
    mediaTrack = match.group(1)
    timeScale = float(match.group(2))
    before = float(match.group(3))
    after = float(match.group(4))
    duration = float(match.group(5))
    url = match.group(6).decode()

    segment_cnt += 1
    print(segment_cnt, mediaTrack, timeScale, before, after, duration, url)

    # Our expected pts value starts from 0
    expected_restamp = restamp_values.get(mediaTrack, 0)

    # The actual duration in the provided segments may not match that from the manifest.
    # This can be seen in https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd
    # We allow the pts value after restamp to differ by 5% of the segment duration
    duration_sec = duration / timeScale
    tolerance_sec = duration_sec * 0.05
    after_sec = after / timeScale
    diff_sec = abs(after_sec - expected_restamp)
    print(f"PTS (secs): actual {after_sec:.3f}, expected {expected_restamp:.3f}, diff {diff_sec:.3f}, tol {tolerance_sec:.3f}")
    assert diff_sec <= tolerance_sec

    # Save what we are expecting for the next value
    restamp_values[mediaTrack] = after_sec + duration_sec

    # Exit playback if we have done enough
    if max_segment_cnt != 0 and segment_cnt > max_segment_cnt:
        aamp.sendline("stop")
        max_segment_cnt = 0 # so we do not send multiple stops


TESTDATA0 = {
"title": "Verify continuous PTS restamp on various streams",
"max_test_time_seconds": timeout,
"url": "Various streams",
"aamp_cfg": "info=true\nenablePTSReStamp=true\nprogress=true\n",

"expect_list": [
    {"expect": r"aamp_tune","min":0, "max":2},
    {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-]+)\r\n',"min":0, "max":timeout, "callback" : check_restamp},
    {"expect": r'AAMPGstPlayer_EndOfStreamReached',"min":0, "max":timeout, "end_of_test": True},
    {"expect": r'AAMPGstPlayer_Stop', "min": 0, "max": timeout, "end_of_test": True},
    {"expect": r'StopInternal', "min": 0, "max": timeout, "end_of_test": True},
    ]
}
@pytest.fixture(params=manifest_list)
def test_data(request):
    return request.param

def test_8003_0(aamp_setup_teardown, test_data):
    global segment_cnt, restamp_values, max_segment_cnt, aamp
    segment_cnt = 0
    restamp_values = {}
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))

    TESTDATA0['url'] = test_data['url']
    TESTDATA0['title'] = "Verify continuous PTS restamp on " + test_data['url']
    max_segment_cnt = test_data['expected_restamps']

    aamp.run_expect_b(TESTDATA0)

    assert segment_cnt >= max_segment_cnt
