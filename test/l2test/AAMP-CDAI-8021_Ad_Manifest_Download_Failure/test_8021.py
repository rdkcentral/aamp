#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

# Starts WindowServer, a web server for serving test streams
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

# TestCase10.1: Single source period substituted with single CDAI ad, where ad manifest download fails - Refer TC https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when the single ad manifest is inserted that too the manifest is getting failed.
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 30 seconds long, containing no ads
# The test ensures that the ad is correctly inserted in Period 1 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.
# The test ensures that if the ad manifest is got failed, then it plays the corresponding source period instead of the ad.
 
TESTDATA1 = {
    "title": "Ad manifest download failure",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        # Add a 30-second ad to the stream at the beginning of Period 1
        # Adding the invalid url to forcefully execute the manifest failure scenario
        "advert add http://localhost:8080/content/ad_300s.mpd 30",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:"},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]"},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled"},
        {"expect": r"\[GetAdMPD\]\[\d+\]\[CDAI\] Error on manifest fetch"},
        {"expect":r"\[FulFillAdObject\]\[\d+\] Failed to get Ad MPD\[http://localhost:8080/content/ad_300s.mpd\]"}, 
        {"expect": r"\[CheckForAdStart\]\[\d+\]\[CDAI\]: PlacementObj open adbreak() and current period's(1) adbreak(1) not equal ... may be BUG"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Got adIdx\[-1\] for adBreakId\[1\] but adBreak object exist"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: ads.size() = 1 breakId = 1 mBasePeriodId = 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: AdBreak\[1\] is invalidated. Skipping."},
        {"expect": re.escape("Period ID changed from '0' to '1' [BasePeriodId='1']")},
        # Ensure fragments fetched from period 1
        {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_016.m4s"},
        # Confirm period change to period 2
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']")},
        # Ensure fragments fetched from period 2
	    {"expect":r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_031.m4s"}, 
        # End of the test - confirm some of the fragments are fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_036.m4s\?live=true", "end_of_test":True},
    ]
}
# TestCase10.2: Single source period substituted with multiple CDAI ads, where one ad manifest download fails - Refer TC https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.(With 3 10s ad and one is failing)
# - Period 2: 30 seconds long, containing no ads
# The test ensures that the ad is correctly inserted in Period 1 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.
# The test ensures that the if any one of ad manifest from the multiple ads getting failed then it should check the other manifest is
# valid and then play that corresponding manifest

TESTDATA2 = {
    "title": "Ad manifest download failure with break level substitution",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        # Add 3, 10-second ad to Period 1
        "advert add http://localhost:8080/content/ad_10s.mpd 10",
        # Adding the invalid url to forcefully execute the manifest failure scenario
        "advert add http://localhost:8080/content/ad_100s.mpd 10",
        "advert add http://localhost:8080/content/ad_10s.mpd 10",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:"},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\] isDAIEvent\[1\]"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] New Ad successfully for periodId : 1 added\[Id=adId1, url=http://localhost:8080/content/ad_10s.mpd, durationMs=10000\]"},
        {"expect": r"\[GetAdMPD\]\[\d+\]\[CDAI\] Error on manifest fetch"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] Failed to get Ad MPD\[http://localhost:8080/content/ad_100s.mpd\]"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] New Ad successfully for periodId : 1 added\[Id=adId3, url=http://localhost:8080/content/ad_10s.mpd, durationMs=10000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},

        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].","min":20,"max":40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Current Ad completely placed.end period:1 end period offset:20000 adjustEndPeriodOffset:1"},

        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Current Ad placement Completed. Ready to play next Ad"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[2\] Found at Period\[1\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=2\] \[period=1\]"},
        # Ensure we move to the last 3rd ad in the ad break
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='1']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\]","min":30,"max":50},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:20000 adjustEndPeriodOffset:1"},
        # Ensure placement is completed for the ad break
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 1, endPeriodOffset: 20000, \#Ads: 3"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[20.000000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] => \[OUTSIDE_ADBREAK\]."},
        {"expect": r"\[getValidperiodIdx\]\[\d+\]\[CDAI\]: Landed at period \(1\) periodIdx: 1 duration\(ms\):24000.000000"},
        # Ensure we move back to source period to play the remaining content
        {"expect": re.escape("Period ID changed from '1-114' to '1' [BasePeriodId='1']")},
        # Expectation to check fragment downloaded from Period 1
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_026.m4s\?live=true"},   
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']")},
        # End of the test - confirm some of the fragments are fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_036.m4s\?live=true", "end_of_test":True},
    ]
}
# TestCase10.2: Single source period substituted with multiple CDAI ads, where one ad manifest download fails - Refer TC https://etwiki.sys.comcast.net/pages/viewpage.action?spaceKey=RDKV&title=AAMP+Client-side+Dynamic+Ad+Use+cases
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.(With 3 10s ad and one is failing)
# - Period 2: 30 seconds long, containing no ads
# The test ensures that the if any one of ad manifest from the multiple ads getting failed then it should check the other manifest is
# valid and then play that corresponding manifest
# In this test case the very first ad manifest is failed then it check the corresponding next ad manifest and then it plays that ad manifest

TESTDATA3 = {
    "title": "Ad manifest download failure",
    "max_test_time_seconds": 200,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
    	# Add a 3,10-second ad to the stream at the of Period 1
        # Adding the invalid url to forcefully execute the manifest failure scenario.
        "advert add http://localhost:8080/content/ad_100s.mpd 10",
        "advert add http://localhost:8080/content/ad_10s.mpd 10",
        "advert add http://localhost:8080/content/ad_10s.mpd 10",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:"},
        {"expect": r'RestampPts.*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/\.\-\?=]+)\r\n', "callback" : pts_restamp_utils.check_restamp},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\] isDAIEvent\[1\]"},
        {"expect": r"\[GetAdMPD\]\[\d+\]\[CDAI\] Error on manifest fetch"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] Failed to get Ad MPD\[http://localhost:8080/content/ad_100s.mpd\]"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] Next available DAI Ad break = 1"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] New Ad successfully for periodId : 1 added\[Id=adId2, url=http://localhost:8080/content/ad_10s.mpd, durationMs=10000\]"},
        {"expect": r"\[FulFillAdObject\]\[\d+\]\[CDAI\] New Ad successfully for periodId : 1 added\[Id=adId3, url=http://localhost:8080/content/ad_10s.mpd, durationMs=10000\]"},
        {"expect": r"\[PlaceAds\]\[\d+\]periodDelta = 4000.000000 p2AdData.duration = \[4000\] mPlacementObj.adNextOffset = 0 periodId = 1 mPlacementObj.curAdIdx = 0"},
        {"expect": r"\[PlaceAds\]\[\d+\]curAd.duration = \[0\] mPlacementObj.curAdIdx = 0"},
        {"expect": r"\[GetNextAdInBreakToPlace\]\[\d+\]\[CDAI\] Current Ad Index to be placed\[1\] is valid"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[1\] Found at Period\[1\]"},
        # State change indicating the start of ad playback inside the ad break
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
         
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=1\] \[period=1\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] \=\> \[IN_ADBREAK_WAIT2CATCHUP\].","min":20,"max":40},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Current Ad placement Completed. Ready to play next Ad."},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[2\] Found at Period\[1\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[IN_ADBREAK_AD_PLAYING\]."},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='1']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\].","min":30,"max":50},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:1 end period offset:20000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 1, endPeriodOffset: 20000, \#Ads: 3"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[1\] at Offset\[20.000000\]"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] => \[OUTSIDE_ADBREAK\]"},
        {"expect": re.escape("Period ID changed from '1-114' to '1' [BasePeriodId='1']")},

        # Expectation to check fragment downloaded from Period 1
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_026.m4s\?live=true"},
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_027.m4s\?live=true"},
        # Expectation for period ID change after completing the ad break
        {"expect": re.escape("Period ID changed from '1' to '2' [BasePeriodId='2']")},
        #End of the test - confirm the last segment fetched from Period 2
        {"expect": r"HttpRequestEnd.*?(1080|720|480|360)p_036.m4s\?live=true", "end_of_test":True},  
        ]
}

TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8021(aamp_setup_teardown, test_data):
    global pts_restamp_utils
    aamp = None

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 60
 
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    pts_restamp_utils.check_num_segments()
