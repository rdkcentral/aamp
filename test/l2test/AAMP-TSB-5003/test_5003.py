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

# Starts simlinear, a web server for serving ABR test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
# Also see README.md

import os
from inspect import getsourcefile
import pytest


AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"
archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

AD_URLS = [
    # 40sec - ad1 (telecoms)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd",

    # 60sec - ad2 (lifeboat)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd",

    # 30sec - ad3 (bet)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd",

    # 10sec - ad4 (movie)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd",

    # 20sec - ad5 (witness)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad5/hsar1195-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad2/d14dff37-36d1-4850-aa9d-7d948cbf1fc6/1628318436178/AD/HD/manifest.mpd",

    # 25sec - ad6 (one)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad6/hsar1103-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD/manifest.mpd",

    # 5sec - ad7 (golf)
    AD_HOST + "/VideoTestStream/public/aamptest/streams/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd",
    ]

test_time = 300
aamp = None

# Callbacks used by the tests
def send_command(match, command):
    global aamp
    aamp.sendline(command)

"""
The following callabcks to track adverts segments as they are written, read, then deleted from TSB store
"""
store = {}
def record_tsb_store(match, args):
    global store
    action = match.group(1)
    file = match.group(2)

    #Only interested in the advertising files
    if "ipvod" in file:
        print("TSB Store: ", action, file)
        seg = store.get(file,{'Read':0,'Write':0,'Delete':0})
        seg[action] += 1
        store[file] = seg

def check_tsb_store():
    global store
    for file in store:
        #Headers get written to store but sometimes read out of cache hence not read from store.
        if "header" not in file:
            assert store[file]['Read'] >0, f"File was not read from store {file}"
        assert store[file]['Delete'] == store[file]['Write'], f"File write and delete not equal {file}"


#Period 881036617: 30-second duration with scte35, replaced with 30-second ad
#Period 881036618: 20-second duration no scte35,   replaced with 20-second ad
#Period 881036619: 20-second duration with scte35, replaced with 20-second ad
#Period 881036620: 10-second duration no scte35,   no ad to be placed
#Period 881036621: 30-second ad break with scte35, replaced with 30-second ad
#Period 881036622: 7 sec
#Period 881036623:        startNumber=881045381

"""
Test repeating Ads in the TSB store.
ntsbLength=60 seconds hence the first occurance of the 30Sec ad will deleted from the store
before the 2nd occurance of that ad is written to the store.
The 20sec ad will be written to the store once and read out 2x before deletion 

"""
TESTDATA0 = {
"title": "Linear CDAI with TSb Repeating Ads",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": test_time,
"aamp_cfg": "info=true\ntrace=true\ntsbLength=60\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\nclient-dai=true\nenablePTSReStamp=true\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036618 " + AD_URLS[4], # 20s
    "advert map 881036619 " + AD_URLS[4], # 20s
    "advert map 881036621 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [
	# Wait 5s and then seek to start to get behind live edge and TSB store working
	{"expect": r'Returning Position as 1691602650\d\d\d', "max": 20, "callback_once": send_command, "callback_arg": "seek 0"},
    {"expect": r"Period ID changed.*\]"},
    # Period 881036617 - 30sec

    {"expect": r'\[(Write)Buffer\]\[TsbStore.cpp.*file="(.*?)"', "callback": record_tsb_store},
    {"expect": r'\[(Read)\]\[TsbStore.cpp.*file="(.*?)"', "callback": record_tsb_store},
    {"expect": r'\[(Delete)\]\[TsbStore.cpp.*file="(.*?)"', "callback": record_tsb_store},

    #Trying to write -header.mp4 again OK, trying to write segment again should fail.
    {"expect": r'TsbStore.cpp.*?File already exists.*-frag-', "not_expected": True},
    #We need to run for 60 secs so that all ad segments get deleted from tsb store startNumber=881045381 +60 = ...5441
    {"expect": r"\[Read\].*periodid-881036623.*881045441.mp4", "end_of_test":True} # Finish when we start fetching for this period,

    ]
}


TESTLIST = [TESTDATA0]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

@pytest.mark.ci_test_set
def test_5003(aamp_setup_teardown, test_data):
    global aamp

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    check_tsb_store()
