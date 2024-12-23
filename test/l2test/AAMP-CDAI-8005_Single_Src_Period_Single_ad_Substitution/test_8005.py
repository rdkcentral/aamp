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

# Starts simlinear, a web server for serving test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
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
import sys
from inspect import getsourcefile
import pytest
import re
from l2test_pts_restamp import PtsRestampUtils
from l2test_window_server import WindowServer

###############################################################################

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

pts_restamp_utils = PtsRestampUtils()


# TestCase1.1: Single source period CDAI substitution - Refer TC https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 30 seconds long, containing no ads
# The test ensures that the ad is correctly inserted in Period 1 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.

TESTDATA1 = {
    "title": "Single source period CDAI substitution",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
    	# Add a 30-second ad to the stream at the beginning of Period 1
        "advert add http://localhost:8080/content/ad_30s.mpd 30",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n',"min":0, "max":300, "callback" : pts_restamp_utils.check_restamp},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Confirm the transition from the content to the ad by checking the period ID change
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 180},
        # Verify the state changes back to OUTSIDE_ADBREAK after completing the ad
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 180},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 10, "max": 180},
        #After completing the ad, confirm the transition back to Period 2 (base content)
	{"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 20, "max": 180},
        #Ensure that the first segment of ad is correctly fetched
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_1.m4s", "min": 20, "max": 180},
        #Ensure that the last  segment of ad is correctly fetched 
	{"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_015.m4s", "min": 20, "max": 180},
	#Ensure fragments fetched from period 2
	{"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_031.m4s","min":20,"max":180}, 
	
        #End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_045.m4s\?live=true", "min": 0, "max": 180, "end_of_test":True},
    ]
}

#1.2.Back to back single source period CDAI substitution - Covered in TST_2001_CDAI_Linear TestDATA1


TESTLIST = [TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8005(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()
