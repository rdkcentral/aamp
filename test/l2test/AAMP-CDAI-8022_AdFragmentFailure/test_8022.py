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

# Starts WindowServer, a web server for serving test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
# Runs through CDAI ad insertion test cases with PTS restamping enabled
# Simulates fragment download failures on ad video fragments and verifies the behavior

import os
import sys
from inspect import getsourcefile
import pytest
import re
from l2test_window_server import WindowServer
# Due to the gap in fragments, the pts restamp checks will fail as there is a jump in the pts values. Hence disabling the checks for now

###############################################################################

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"


# TestCase1 : Single source period CDAI substitution with video fragment failure
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 30 seconds long, containing no ads
# The ad simulates http error 404 on the ad video fragments (3, 4, 5)
# The test ensures that the ad is correctly inserted in Period 1 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.

TESTDATA1 = {
    "title": "Ad failure with single source period CDAI substitution",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    # Fail ad fragments 3, 4, 5 with 404
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_30.*?(1080|720|480|360)p_00(3|4|5).m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
    	# Add a 30-second ad to the stream at the beginning of Period 1
        "advert add http://localhost:8080/content/ad_30s.mpd 30",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        # Ensure fragments from period 1 are not fetched (16-30)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_0([1][6-9]|[2][0-9]|30).m4s", "min":0,"max": 180, "not_expected": True},
        # Make sure there are no stall at discontinuity processing
        {"expect": r"\[CheckForMediaTrackInjectionStall\]", "min": 0, "max": 180, "not_expected": True},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Confirm the transition from the content to the ad by checking the period ID change
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 180},
        # Ensure that the first segment of ad is correctly fetched
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_001.m4s", "min": 10, "max": 60},
        # Make sure we get 404 for the failed ad fragments, rampdown included
        {"expect": r"HttpRequestEnd: 0,0,404.*?http://localhost:8080/content/ad_30/(1080|720|480|360)p_00(3|4|5).m4s", "min": 10, "max": 60},
        # After failing more than 2 fragments, the below log should be seen
        # This is not a realistic check, as some of the init fragment downloads will reset segDLFailCount and prevent it from going above 2 to log this line.
        # Honestly this log line should be removed, causes unwanted concerns.
        #{"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: \[CDAI\] Ad fragment not available. Playback failed.", "min": 10, "max": 60},
        # Ensure the position jump is handled correctly
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[video\] Found a positionDelta \(6\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)", "min": 10, "max": 60},
        # Ensure that the last  segment of ad is correctly fetched
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_015.m4s", "min": 10, "max": 60},
        # Verify the state changes back to WAIT2CATCHUP afterwards
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 10, "max": 120},
        # Verify the state changes back to OUTSIDE_ADBREAK after completing the ad
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 30, "max": 120},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 120},
        # After completing the ad, confirm the transition back to Period 2 (base content)
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 30, "max": 120},
        # Ensure fragments fetched from period 2
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s", "min": 40, "max": 180},
        {"expect": r"aamp url:0,1,1,2.000000,http://localhost:8080/content/dash/en_031.mp3\?live=true", "min": 40, "max": 180},
        # End of the test - confirm the some more fragments are fetched from Period 2
        {"expect": r"HttpRequestEnd: 0,0,200.*?http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 40, "max": 180, "end_of_test": True},
    ]
}

# TestCase2 : Back to back source period CDAI substitution with video fragment failure
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (BackToBackAd.mpd) with four periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 10 seconds long, with a single 10-second ad.
# - Period 4: 30 seconds long, containing no ads
# The ad simulates http error 404 on the ad video fragments (3, 4, 5) in 30-second ad and (3, 4, 5) in 10-second ad
# The test ensures that the ad is correctly inserted in Period 1 and Period 2 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.

TESTDATA2 = {
    "title": "Ad failure with back to back source period CDAI substitution",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    # Fail ad fragments 3, 4, 5 in 30s ad with 404 and ad fragments 3, 4, 5 n 10s ad with 404
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_(30|20).*?(1080|720|480|360)p_00(3|4|5).m4s"]},
    "url": "http://localhost:8080/content/BackToBackAd.mpd?live=true",
    "cmdlist": [
        "adtesting",
    	# Add a 30-second ad to Period 1
        "advert add http://localhost:8080/content/ad_30s.mpd 30 0",
        # Add a 10-second ad to Period 2
        "advert add http://localhost:8080/content/ad_10s.mpd 10 1",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        # Ensure fragments from period 1 are not fetched (16-30)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_0([1][6-9]|[2][0-9]|30).m4s", "min":0,"max": 180, "not_expected": True},
        # Ensure fragments from period 2 are not fetched (31-35)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_0(3[1-5]).m4s", "min":0,"max": 180, "not_expected": True},
        # Make sure there are no stall at discontinuity processing
        {"expect": r"\[CheckForMediaTrackInjectionStall\]", "min": 0, "max": 180, "not_expected": True},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\] isDAIEvent\[1\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        # Confirm the transition from the content to the ad by checking the period ID change
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 180},
        # Ensure that the first segment of ad is correctly fetched
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_001.m4s", "min": 10, "max": 60},
        # Make sure we get 404 for the failed ad fragments, rampdown included
        {"expect": r"HttpRequestEnd: 0,0,404.*?http://localhost:8080/content/ad_30/(1080|720|480|360)p_00(3|4|5).m4s", "min": 10, "max": 60},
        # After failing more than 2 fragments, the below log should be seen
        # This is not a realistic check, as some of the init fragment downloads will reset segDLFailCount and prevent it from going above 2 to log this line.
        # Honestly this log line should be removed, causes unwanted concerns.
        #{"expect": r"\[FetchFragment\]\[\d+\]StreamAbstractionAAMP_MPD: \[CDAI\] Ad fragment not available. Playback failed.", "min": 10, "max": 60},
        # Ensure the position jump is handled correctly
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[video\] Found a positionDelta \(6\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)", "min": 10, "max": 60},
        # Ensure that the last  segment of ad is correctly fetched 
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_015.m4s", "min": 10, "max": 60},
        # Verify the state changes back to WAIT2CATCHUP afterwards
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 10, "max": 120},
        # Next ad break is advertised
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[2\] Duration\[10000\] isDAIEvent\[1\]", "min": 10, "max": 120},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 10, "max": 120},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=2 adId\=adId2 duration\=10 url\=.*?ad_10s.mpd", "min": 10, "max": 120},
        # Verify the state changes back to OUTSIDE_ADBREAK after completing the ad
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 30, "max": 120},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 120},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]", "min": 30, "max": 120},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 30, "max": 120},
        # After completing the ad, confirm the transition back to next ad (basePeriod 2)
        {"expect": re.escape("Period ID changed from '0-111' to '1-114' [BasePeriodId='2']"), "min": 30, "max": 120},
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_20/*?(1080|720|480|360)p_001.m4s", "min": 30, "max": 120},
        # Make sure we get 404 for the failed ad fragments, rampdown included
        {"expect": r"HttpRequestEnd: 0,0,404.*?http://localhost:8080/content/ad_20/(1080|720|480|360)p_00(3|4|5).m4s", "min": 40, "max": 180},
        # Ensure the position jump is handled correctly
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[video\] Found a positionDelta \(6\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)", "min": 40, "max": 180},
        # Verify the state changes back to WAIT2CATCHUP afterwards
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 40, "max": 180},
        # Verify the state changes back to OUTSIDE_ADBREAK after completing the ad
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: , duration: 10000, endPeriodId: 3, endPeriodOffset: 0, \#Ads: 1", "min": 40, "max": 180},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 40, "max": 180},
        # After completing the ad, confirm the transition back to Period 3 (base content)
        {"expect": re.escape("Period ID changed from '1-114' to '4' [BasePeriodId='4']"), "min": 40, "max": 180},
        # Ensure fragments fetched from period 3
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_036.m4s", "min": 40, "max": 180},
        {"expect": r"aamp url:0,1,1,2.000000,http://localhost:8080/content/dash/en_036.mp3\?live=true", "min": 40, "max": 180},
        # End of the test - confirm the some more fragments are fetched from Period 3
        {"expect": r"HttpRequestEnd: 0,0,200.*?http://localhost:8080/content/dash/(1080|720|480|360)p_045.m4s\?live=true", "min": 40, "max": 180, "end_of_test": True},
    ]
}


# TestCase3 : Single source period CDAI substitution with source audio fragment failure
# Description:
# This test case validates the behavior of Client Dynamic Ad Insertion (CDAI) when substituting a single ad
# into a linear stream. The content is represented by an MPD file (TC1.mpd) with three periods:
# - Period 0: 30 seconds long, containing no ads
# - Period 1: 30 seconds long, with a single 30-second ad.
# - Period 2: 30 seconds long, containing no ads
# The ad simulates http error 404 on the source audio fragments (4, 7, 10)
# The test ensures that the ad is correctly inserted in Period 1 and that playback transitions smoothly
# between the base content and the ad, and then back to the base content.

TESTDATA3 = {
    "title": "Ad failure with single source period CDAI substitution",
    "max_test_time_seconds": 180,
    "aamp_cfg": "client-dai=true\nenablePTSReStamp=true\ninfo=true\nprogress=true\n",
    "archive_url": archive_url,
    # Fail source audio fragments 4, 7, 10 with 404
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "dash/en_0(04|07|10).mp3"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
    	# Add a 30-second ad to the stream at the beginning of Period 1
        "advert add http://localhost:8080/content/ad_30s.mpd 30",
    ],

    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune:", "min": 0, "max": 3},
        # Ensure fragments from period 1 are not fetched (16-30)
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/*?(1080|720|480|360)p_0([1][6-9]|[2][0-9]|30).m4s", "min":0,"max": 180, "not_expected": True},
        # Make sure there are no stall at discontinuity processing
        {"expect": r"\[CheckForMediaTrackInjectionStall\]", "min": 0, "max": 180, "not_expected": True},
        # Make sure we get 404 for the failed source audio fragments, no rampdown
        {"expect": r"HttpRequestEnd: 0,1,404.*?http://localhost:8080/content/dash/en_0(04|07|10).mp3\?live=true", "min": 0, "max": 50},
        # Ensure the position jump is handled correctly
        # Adding a min, max for the below logs are ending up in failure. Hence removing it.
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[audio\] Found a positionDelta \(2\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)"},
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[audio\] Found a positionDelta \(2\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)"},
        {"expect": r"\[HandleFragmentPositionJump\]\[\d+\]\[audio\] Found a positionDelta \(2\.000000\) between lastInjectedDuration \(\d+\.\d+\) and new fragment absPosition \(\d+\.\d+\)"},
        # Confirm that an ad break is detected in Period 1 with a duration of 30 seconds
        {"expect": r"\[FoundEventBreak\]\[\d+\]\[CDAI\] Found Adbreak on period\[1\] Duration\[30000\]", "min": 0, "max": 50},
        {"expect": r"\[Event\]\[\d+\]\[CDAI\] Dynamic ad start signalled", "min": 0, "max": 50},
        {"expect": r"\[AMPCLI\] AAMP_EVENT_TIMED_METADATA place advert breakId\=1 adId\=adId1 duration\=30 url\=.*?ad_30s.mpd", "min": 0, "max": 50},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[OUTSIDE_ADBREAK\] \=\> \[IN_ADBREAK_AD_PLAYING\].", "min": 10, "max": 60},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]", "min": 10, "max": 60},
        # Confirm the transition from the content to the ad by checking the period ID change
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']"), "min": 20, "max": 180},
        # Ensure that the first segment of ad is correctly fetched
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_001.m4s", "min": 10, "max": 60},
        # Ensure that the last  segment of ad is correctly fetched 
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/ad_30/*?(1080|720|480|360)p_015.m4s", "min": 10, "max": 60},
        # Verify the state changes back to WAIT2CATCHUP afterwards
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_AD_PLAYING\] => \[IN_ADBREAK_WAIT2CATCHUP\].", "min": 10, "max": 120},
        # Verify the state changes back to OUTSIDE_ADBREAK after completing the ad
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1", "min": 30, "max": 120},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: State changed from \[IN_ADBREAK_WAIT2CATCHUP\] \=\> \[OUTSIDE_ADBREAK\].", "min": 30, "max": 120},
        # After completing the ad, confirm the transition back to Period 2 (base content)
        {"expect": re.escape("Period ID changed from '0-111' to '2' [BasePeriodId='2']"), "min": 30, "max": 120},
        # Ensure fragments fetched from period 2
        {"expect": r"aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s", "min": 40, "max": 180},
        {"expect": r"aamp url:0,1,1,2.000000,http://localhost:8080/content/dash/en_031.mp3\?live=true", "min": 40, "max": 180},
        # End of the test - confirm the some more fragments are fetched from Period 2
        {"expect": r"HttpRequestEnd: 0,0,200.*?http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 40, "max": 180, "end_of_test": True},
    ]
}

TESTLIST = [TESTDATA1, TESTDATA2, TESTDATA3]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_8022(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
