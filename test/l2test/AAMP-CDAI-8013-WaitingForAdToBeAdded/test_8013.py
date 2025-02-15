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
# Also see README.md

import os
import sys
from inspect import getsourcefile
import pytest

archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

AD_URL = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"

###############################################################################


@pytest.mark.ci_test_set
def test_8013(aamp_setup_teardown):
    '''Tests the OUTSIDE_ADBREAK_WAIT4ADS state'''
    # This test runs a stream with period 881036616 with 20sec duration
    # It then adds an ad placement to period 881036617 with 30sec duration
    # But the ad is only added when download head reaches baseperiod 881036617 with a delay
    BASIC_TEST_DATA = {
        "title": "Linear CDAI TESTDATA OUTSIDE_ADBREAK_WAIT4ADS",
        "archive_url": archive_url,
        "url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
        'simlinear_type': 'DASH',
        "max_test_time_seconds": 60,
        "aamp_cfg": "info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\n",
        "cmdlist": [ "advert list" ],
        "expect_list": [
                {"expect": r"Found CDAI events for period 881036617"},
                {"expect": r"\[onAdEvent\]\[\d+\]\[CDAI\] Got adIdx\[-1\] for adBreakId\[881036617\] but adBreak object exist"},
                {"expect": r"\[WaitForNextAdResolved\]\[\d+\]Waiting for next ad placement in 881036617 to complete with timeout \d+ ms."},
                {"cmd": "advert clear"},
                {"cmd": "set alternateContents 881036617 ad1 " + AD_URL},
                {"expect": r"State changed from \[OUTSIDE_ADBREAK\] => \[OUTSIDE_ADBREAK_WAIT4ADS\]"},
                {"expect": r"State changed from \[OUTSIDE_ADBREAK_WAIT4ADS\] => \[IN_ADBREAK_AD_PLAYING\]"},
                {"expect": r"\[SelectSourceOrAdPeriod\]\[\d+\]Period ID changed from '881036616' to '0-1' \[BasePeriodId='881036617'\]"},
                {"expect": r"HttpRequestEnd: 2,7,.*--header.mp4"},
                {"expect": r"HttpRequestEnd: 0,0,.*frag-0.mp4"},
                {"expect": "Found CDAI events for period 881036618"},
                {"cmd": "stop"},
        ]
    }
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(BASIC_TEST_DATA)


