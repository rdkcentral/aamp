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
    { 'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/misc/main-segmentbase.mpd", 'expected_restamps': 40},
    { 'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main-multi.mpd", 'expected_restamps': 40},
    { 'url': "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/RDKAAMP-1540/4k/h265/SegmentTimeline.mpd", 'expected_restamps': 40},
]
###############################################################################
restamp_values = {}
segment_cnt = 0
max_segment_cnt = 0
aamp = None

def check_restamp(match,arg):
    global segment_cnt, restamp_values

    # Get the fields from the log line
    before = int(match.group(1))
    after = int(match.group(2))
    duration = int(match.group(3))
    url = match.group(4).decode()

    segment_cnt += 1
    print(segment_cnt, before,after,duration, url)

    # Identify type of segment. comcast generated segments need a pattern match
    # This turns out to be a rubbish approach better if there was a log line
    if re.search(r'_video_|-video-|/dash/\d+p_\d+\.m4s|/dash/\d+\.mp4|/video/\d+/\d+\.m4s', url):
        media = 'video'
    elif re.search(r'_audio_|-audio-|/dash/en_\d+\.mp3|/en/\d+\.m4s', url):
        media = 'audio'
    elif ("-subtitle-" in url) or ("-text-" in url):
        media = 'subtitle'
    else:
        assert False, f"Cannot establish media from {url}"

    # Our expected pts value starts from 0
    expected_restamp = restamp_values.get(media,0)

    # The actual duration in the provided segments may not match that from the manifest.
    # This can be seen in https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd
    # We allow the pts value after restamp to differ by 5% of the segment duration
    tolerance = duration*0.05
    if abs( after - expected_restamp )> tolerance:
        assert(after == expected_restamp), f"tolerance={tolerance} {media} {url}"

    # Save what we are expecting for the next value
    restamp_values[media] = after + duration

# Limit playback for each asset so test does not take too long
def check_runtime(match,arg):
    global max_segment_cnt, segment_cnt, aamp
    if max_segment_cnt != 0 and segment_cnt > max_segment_cnt:
        aamp.sendline("stop")
        max_segment_cnt = 0 # so we do not send multiple stops

TESTDATA0 = {
"title": "TBD",
"max_test_time_seconds": 60,
"url": "TBD",
"aamp_cfg": "info=true\nenablePTSReStamp=true\nprogress=true\n",

"expect_list": [
    {"expect": r"aamp_tune","min":0, "max":2},
    {"expect": r'RestampPts.*?before (\d+) after (\d+) duration (\d+) ([\w:/\.\-]+)\r\n',"min":0, "max":60, "callback" : check_restamp},
    {"expect": r'GetPositionMilliseconds', "callback" : check_runtime, "min": 0, "max": 60},
    {"expect": r'AAMPGstPlayer_EndOfStreamReached',"min":0, "max":60, "end_of_test": True},
    {"expect": r'AAMPGstPlayer_Stop', "min": 0, "max": 60, "end_of_test": True},
    {"expect": r'StopInternal', "min": 0, "max": 60, "end_of_test": True},
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
    TESTDATA0['title'] = test_data['url']
    max_segment_cnt = test_data['expected_restamps']

    aamp.run_expect_b(TESTDATA0)

    assert segment_cnt >= max_segment_cnt