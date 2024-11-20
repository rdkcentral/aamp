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
# Also see README.md

import os
import sys
from inspect import getsourcefile
import pytest
from l2test_window_server import WindowServer

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"


# TestCase1.1: Single source period CDAI substitution - Refer TC https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 60 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 10 seconds long, containing a single 10-second ad.
# - Period 3: 30 seconds long, containing a single 30-second ad.
# - Period 4: 20 seconds long, containing a single 20-second ad.
# - Period 5: 30 seconds long, containing a single 30-second ad.
# - Period 6: 20 seconds long, containing no ads
# The test ensures that the ad is correctly inserted in Period 1-5 with proper AD_EVENTS and that playback transitions smoothly
# between the base content and the ads, and then back to the base content with proper AdEvent delivery

TESTDATA1 = {
    "title": "Linear CDAI TESTDATA DeliverADEvents",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/CDAI_Test.mpd?live=true",
    "max_test_time_seconds": 220,
    "aamp_cfg": "info=true\nprogress=true\ntrace=true\nlogMetadata=true\nclient-dai=true\nenablePTSReStamp=true\nuseAbsoluteTimeline=true\nenableSeekableRange=true\npreferredAbsoluteReporting=0\nprogressReportingInterval=0.25\n",
    "cmdlist": [
        # Add a 30-second ad to the stream at the beginning of Period 1
        "advert add http://localhost:8080/content/ad_30s.mpd 30",
        "advert add http://localhost:8080/content/ad_20s.mpd 20",
        "advert add http://localhost:8080/content/ad_10s.mpd 10",
        ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[\d+\] aamp_tune:", "min": 0, "max": 30},
        {"expect": r"Found CDAI events for period", "min": 0, "max": 200},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '0' to '0-111' \[BasePeriodId='1'\]", "min": 0, "max": 80},
        {"expect": r"HttpRequestEnd: 2,7,.*_init.m4s\?live=true\r\n", "min": 0, "max": 180},
        {"expect": r"HttpRequestEnd: 0,0,.*_001.m4s\?live=true\r\n", "min": 0, "max": 180},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=\d+\toffset=0\tduration=30000\terror=0", "min": 0, "max": 100},
        {"expect": r"\[ReportAdProgress\]\[\d+\]AdId:adId1\s+pos:\s+\d+..\d+.\d+..\d+..(9\d+.\d{2}|100)%\)", "min": 0, "max": 100},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 0, "max": 100},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '0-111' to '1-114' \[BasePeriodId='2'\]", "min": 0, "max": 120},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2\tposition=\d+\toffset=0\tduration=10000\terror=0", "min": 0, "max": 130},
        {"expect": r"\[ReportAdProgress\]\[\d+\]AdId:adId2\s+pos:\s+\d+..\d+.\d+..\d+..(9\d+.\d{2}|100)%\)", "min": 0, "max": 130},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 0, "max": 150},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '1-114' to '2-111' \[BasePeriodId='3'\]", "min": 0, "max": 150},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_START\tadId=adId3\tposition=\d+\toffset=0\tduration=30000\terror=0", "min": 0, "max": 180},
        {"expect": r"\[ReportAdProgress\]\[\d+\]AdId:adId3\s+pos:\s+\d+..\d+.\d+..\d+..(9\d+.\d{2}|100)%\)", "min": 0, "max": 140},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_END\tadId=adId3\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 0, "max": 180},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '2-111' to '3-114' \[BasePeriodId='4'\]", "min": 0, "max": 180},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_START\tadId=adId4\tposition=\d+\toffset=0\tduration=20000\terror=0", "min": 0, "max": 180},
        {"expect": r"\[ReportAdProgress\]\[\d+\]AdId:adId4\s+pos:\s+\d+..\d+.\d+..\d+..(9\d+.\d{2}|100)%\)", "min": 0, "max": 150},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_END\tadId=adId4\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 0, "max": 180},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '3-114' to '4-111' \[BasePeriodId='5'\]", "min": 0, "max": 180},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_START\tadId=adId5\tposition=\d+\toffset=0\tduration=30000\terror=0", "min": 0, "max": 180},
        {"expect": r"\[ReportAdProgress\]\[\d+\]AdId:adId5\s+pos:\s+\d+..\d+.\d+..\d+..(9\d+.\d{2}|100)%\)", "min": 0, "max": 200},
        {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '4-111' to '6' \[BasePeriodId='6'\]", "min": 0, "max": 200},
        {"expect": r"HttpRequestEnd: 0,0,.*_091.m4s\?live=true\r\n", "min": 0, "max": 200},
        {"expect": r"AAMP_EVENT_AD_PLACEMENT_END\tadId=adId5\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 0, "max": 220, "end_of_test":True},
    ],
}

# DeliverADEvents test data
TESTLIST = [TESTDATA1]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8014(aamp_setup_teardown, test_data):
    '''Tests the DeliverADEvents for each ad placement in the stream'''
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

