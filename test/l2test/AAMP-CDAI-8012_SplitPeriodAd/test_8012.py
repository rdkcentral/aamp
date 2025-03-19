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
import re
import copy
import pytest
from l2test_pts_restamp import PtsRestampUtils
from l2test_window_server import WindowServer

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"
timeout = 180
pts_restamp_utils = PtsRestampUtils()

# Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# 0th period 30s no ads
# 1st period 20 seconds -> 20 second ad
# 2nd period 10 seconds -> 20 second ad
# 3rd period 10 seconds -> no ad
# 4th period 20 seconds -> 20 second ad
# 5th period no ads till end of content
# Expected behaviour : after playing 20 second ad in 2nd period we should see 4th period ad playback for 20 seconds
TESTDATA1 = {
    "title": "Test 1 Split period ad",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_20s.mpd",
    "advert map 4 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split.mpd"},
        #Starting adbreak in 'period 1'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId1 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 2, endPeriodOffset: 0, #Ads: 1"},
        #Starting adbreak in 'period 2'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId2 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='2']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=0\] \[period\=2\]"},
        # Ads started in 'period 2', overlapped in 'period 3', ends in 'period 3'
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:3 end period offset:10000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period.*?adId2-20000", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 20000, endPeriodId: 4, endPeriodOffset: 0, \#Ads: 1"},
        #Expect not to download any fragments from 3rd period
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        #Starting adbreak in 'period 4'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[4\] AdIdx\[0\] Found at Period\[4\]."},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId3 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '1-114' to '2-114' [BasePeriodId='4']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx\=0\] \[period\=4\]"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 4, duration: 20000, endPeriodId: 5, endPeriodOffset: 0, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[4\] FINISHED. Playing the basePeriod\[5\] at Offset\[0.000000\]"},
        {"expect": re.escape("Period ID changed from '2-114' to '5' [BasePeriodId='5']")},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "end_of_test":True},
    ]
}

# Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# 0th period 30s no ads
# 1st period 20 seconds -> 20 second ad
# 2nd period 10 seconds -> 15 second ad
# 3rd period 10 seconds -> no ad
# 4th period 20 seconds -> 20 second ad
# 5th period no ads till end of content
# Expected behaviour : after playing 15 second ad in 2nd period we should see 5 seconds of base content of 3rd period then 4th period ad playback for 20 seconds
TESTDATA2 = {
    "title": "Test 2 Split period ad with short ad",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split2.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 2 http://localhost:8080/content/ad_15s.mpd",
    "advert map 4 http://localhost:8080/content/ad_20s.mpd",
    "advert list",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split2.mpd"},
        #Starting adbreak in 'period 1'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId1 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 20000, endPeriodId: 2, endPeriodOffset: 0, \#Ads: 1"},
         #Starting adbreak in 'period 2'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[2\] AdIdx\[0\] Found at Period\[2\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId2 starts. Duration\=15 sec URL\=http://localhost:8080/content/ad_15s.mpd"},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='2']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Ad finished at Period. Waiting to catchup the base offset.\[idx=0\] \[period\=2\]"},
        # Ads started in 'period 2', overlapped in 'period 3', ends in 'period 3'
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period.*?adId2-15000", "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 2, duration: 15000, endPeriodId: 3, endPeriodOffset: 5000, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[2\] FINISHED. Playing the basePeriod\[3\] at Offset\[5.000000\]."},
        {"expect": re.escape("Period ID changed from '1-114' to '3' [BasePeriodId='3']")},
        #Starting adbreak in 'period 4'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[4\] AdIdx\[0\] Found at Period\[4\]."},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId3 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '3' to '2-114' [BasePeriodId='4']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 4, duration: 20000, endPeriodId: 5, endPeriodOffset: 0, \#Ads: 1"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[4\] FINISHED. Playing the basePeriod\[5\] at Offset\[0.000000\]."},
        {"expect": re.escape("Period ID changed from '2-114' to '5' [BasePeriodId='5']")},
        #Expect to download fragments from 'period 3', after 5sec offset
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_031.m4s\?live=true", "not_expected" : True},
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_062.m4s\?live=true", "min": 50, "end_of_test":True},
    ]
}

# Split period ad across multiple periods
# Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# 0th period 30s no ads
# 1st period 10 seconds -> 30 second ad
# 2nd period 10 seconds -> no ad
# 3rd period 10 seconds -> no ad
# 4th period 20 seconds -> no ad
# 5th period no ads till end of content
# Expected behaviour : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
TESTDATA3 = {
    "title": "Test 3 Split period ad across multiple periods ",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    "advert list",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId1 starts. Duration\=30 sec URL\=http://localhost:8080/content/ad_30s.mpd"},
        {"expect": re.escape("Period ID changed from '0' to '0-111' [BasePeriodId='1']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period.*?adId1-30000", "min": 0, "max": 150},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 4, endPeriodOffset: 0, \#Ads: 1,"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[4\] at Offset\[0.000000\]."},
        {"expect": re.escape("Period ID changed from '0-111' to '4' [BasePeriodId='4']")},
        #Expect to download fragments from 'period 3', after 5sec offset
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true", "not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50, "end_of_test":True},
    ]
}

# Split period ad across multiple periods
# Test content is as follows : described as -> <N>th period <duration of period> seconds -> <scte35 marker duration> seconds ad
# 0th period 30s no ads
# 1st period 10 seconds -> 20 second ad, 10 second ad
# 2nd period 10 seconds -> no ad
# 3rd period 10 seconds -> no ad
# 4th period 20 seconds -> no ad
# 5th period no ads till end of content
# Expected behaviour : after playing 30 second ad in 1ST period ,It should skip period 2,3 and should starts from period 4
TESTDATA4 = {
    "title": "Test 4 Split period ad across multiple periods ",
    "max_test_time_seconds": timeout,
    "aamp_cfg": "client-dai=true\ninfo=true\nprogress=true\ndebug=true\n",
    "archive_url": archive_url,
    'archive_server': {'server_class': WindowServer},
    "url": "http://localhost:8080/content/split3.mpd?live=true",
    "cmdlist": [
    "adtesting",
    "advert map 1 http://localhost:8080/content/ad_20s.mpd",
    "advert map 1 http://localhost:8080/content/ad_10s.mpd",
    "advert list",
    ],
    "expect_list": [
        {"expect": r"\[Tune\]\[\d+\]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: http://localhost:8080/content/split3.mpd"},
        #Starting adbreak in 'period 1'
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[1\] AdIdx\[0\] Found at Period\[1\]"},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId1 starts. Duration\=20 sec URL\=http://localhost:8080/content/ad_20s.mpd"},
        {"expect": re.escape("Period ID changed from '0' to '0-114' [BasePeriodId='1']")},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Current Ad placement Completed. Ready to play next Ad."},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[1\] Found at Period\[1\]."},
        {"expect": r"\[SendAnomalyEvent\]\[\d+\]Anomaly evt:2 msg:\[CDAI\] AdId\=adId2 starts. Duration\=10 sec URL\=http://localhost:8080/content/ad_10s.mpd"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Current Ad completely placed.end period:3 end period offset:10000 adjustEndPeriodOffset:1"},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Detected split period.*?adId1-20000,adId2-10000"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: Next AdIdx\[1\] Found at Period\[1\]."},
        {"expect": re.escape("Period ID changed from '0-114' to '1-114' [BasePeriodId='1']")},
        {"expect": r"\[PlaceAds\]\[\d+\]\[CDAI\] Placement Done: \{AdbreakId: 1, duration: 30000, endPeriodId: 4, endPeriodOffset: 0, \#Ads: 2,"},
        {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[1\] FINISHED. Playing the basePeriod\[4\] at Offset\[0.000000\]."},
        {"expect": re.escape("Period ID changed from '1-114' to '4' [BasePeriodId='4']")},
        #Expect to download fragments from 'period 3', after 5sec offset
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_026.m4s\?live=true","not_expected" : True},
        #Expect to play the remaining content from base period 4
        {"expect": r"\[GetFile\]\[\d+\]aamp url:0,0,0,2.000000,http://localhost:8080/content/dash/(1080|720|480|360)p_040.m4s\?live=true", "min": 50,"end_of_test":True},
    ]
}
TESTLIST = [
              {     'testdata':TESTDATA1,'restamp': True},
              {     'testdata':TESTDATA1,'restamp': False},
              {     'testdata':TESTDATA2,'restamp': True},
              {     'testdata':TESTDATA2,'restamp': False},
              {     'testdata':TESTDATA3,'restamp': True},
              {     'testdata':TESTDATA4,'restamp': True},
]
@pytest.fixture(params=TESTLIST)
def test_list(request):
    return request.param

def test_8012(aamp_setup_teardown, test_list):
    global pts_restamp_utils
    aamp = None

    test_data = copy.deepcopy(test_list['testdata'])
    if test_list.get('restamp',True):
        test_data['expect_list'].append({"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp})
        test_data['aamp_cfg'] += "enablePTSReStamp=true\n"
    else:
        test_data['aamp_cfg'] += "enablePTSReStamp=false\n"

    pts_restamp_utils.reset()
    pts_restamp_utils.tolerance_min = 0.7
    pts_restamp_utils.max_segment_cnt = 20

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)
    if test_list.get('restamp',True):
        pts_restamp_utils.check_num_segments()


