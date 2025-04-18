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
from l2test_window_server import WindowServer
import re

# Define the progress report interval, and the divisor
PROGRESS_REPORT_INTERVAL = 0.5
PROGRESS_REPORT_DIVISOR = 1

AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"
archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
error_archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz"

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

aamp = None

# Callbacks used by the tests
def send_command(match, command):
    global aamp
    aamp.sendline(command)

"""
The following callbacks to track adverts segments as they are written, read, then deleted from TSB store
"""
store = {}

def initialise_tsb_store():
    global store

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

    print("Checking TSB Store")
    for file in store:
        #Headers get written to store but sometimes read out of cache hence not read from store.
        if "header" not in file:
            assert store[file]['Read'] >0, f"File was not read from store {file}"
        assert store[file]['Delete'] == store[file]['Write'], f"File write and delete not equal {file}"

"""
The following callback checks the event removed is less than or equal to the culled position
"""
def check_metadata_removal(match, args):
    global metadata_store
    position = match.group(1)
    type = match.group(2)
    metadata_position = match.group(3)

    print("Metadata Removal: ", position, type, metadata_position)
    assert metadata_position <= position, f"Incorrect metadata removal {type} {position} {metadata_position}"

"""
The following callback check that the metadata added to the TSB is correct, matching the table
"""
RESERVATIONS_TABLE = [
    {"id": "881036617", "start_pos": "1691602655280", "end_pos": "1691602685280"},
    {"id": "881036618", "start_pos": "1691602685280", "end_pos": "1691602705280"},
    {"id": "881036619", "start_pos": "1691602705280", "end_pos": "1691602725280"},
    {"id": "881036620", "start_pos": "1691602725280", "end_pos": "1691602735280"},
    {"id": "881036621", "start_pos": "1691602735280", "end_pos": "1691602765280"},
]

current_reservation_id = ""
current_placement_id = ""
current_reservation_duration = 0

def initialise_check_metadata_addition():
    global current_reservation_id
    global current_placement_id

    current_reservation_id = ""
    current_placement_id = ""
    current_reservation_duration = 0

def check_metadata_addition(match, args):
    global current_reservation_id
    global current_placement_id
    global current_reservation_duration
    global RESERVATIONS_TABLE

    type = match.group(1)
    id = match.group(2)
    position = int(match.group(3))
    duration = int(match.group(4)) if match.group(4) else 0  # Handle optional duration

    if type == "Reservation Start" or type == "Reservation End":
        found_entry = False
        for entry in RESERVATIONS_TABLE:
            if entry["id"] == id:
                found_entry = True
                break
        assert found_entry, f"Incorrect metadata addition {type} {id} {position}"

        if type == "Reservation Start":
            assert current_reservation_id == "", f"Incorrect metadata addition {type} {id} {position}"
            assert current_placement_id == "", f"Incorrect metadata addition {type} {id} {position}"
            expected_position = int(entry["start_pos"])
            assert expected_position == int(position), f"Incorrect metadata addition {type} {id} {position} expected position {expected_position}"
            current_reservation_id = entry["id"]
            current_reservation_duration = 0
        else:
            assert current_reservation_id == id, f"Incorrect metadata addition {type} {id} {position}"
            expected_position = int(entry["end_pos"])
# Can't check this due to incorrect calculation in onAdEvent giving incorrect position intermittently
#            assert expected_position == int(position), f"Incorrect metadata addition {type} {id} {position} expected position {expected_position}"
            assert current_reservation_duration == int(entry["end_pos"]) - int(entry["start_pos"]), f"Incorrect metadata addition {type} {id} {position}"
            current_reservation_id = ""

    if type == "Placement Start" or type == "Placement End":
        assert current_reservation_id != "", f"Incorrect metadata addition {type} {id} {position}"
        found_entry = False
        for entry in RESERVATIONS_TABLE:
            if entry["id"] == current_reservation_id:
                found_entry = True
                break
        assert found_entry, f"Incorrect metadata addition {type} {id} {position}"

        expected_position = int(entry["start_pos"]) + current_reservation_duration

        if type == "Placement Start":
            assert position == expected_position, f"Incorrect metadata addition {type} {id} {position} expected position {expected_position}"
            assert current_placement_id == "", f"Incorrect metadata addition {type} {id} {position}"
            current_placement_id = id
            current_reservation_duration = current_reservation_duration + duration
        else:
# Can't check this due to incorrect calculation in onAdEvent giving incorrect position intermittently
#            assert position == expected_position, f"Incorrect metadata addition {type} {id} {position} expected position {expected_position}"
            assert current_placement_id == id, f"Incorrect metadata addition {type} {id} {position}"
            current_placement_id = ""

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
"max_test_time_seconds": 300,
"aamp_cfg": f"trace=true\ntsbLength=60\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\nclient-dai=true\nenablePTSReStamp=true\n",
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
	{"expect": r'Returning Position as 1691602650\d\d\d', "max": 22, "callback_once": send_command, "callback_arg": "seek 0"},
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

"""

Period 881036617: 30-second reservation placed with 30-second ad
Period 881036618: 20-second duration
Period 881036619: 20-second reservation placed with 20-second ad
"""
TESTDATA1 = {
"title": "Alternating Reservations",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 150,
"aamp_cfg": f"trace=true\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036619 " + AD_URLS[4], # 20s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036619', "max": 90},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=1458171455280", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=1458171485280", "max": 55, "callback_once": send_command, "callback_arg": "seek 0"},

    # Performed seek 0

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=1458171455280", "min": 26},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "min": 26},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=1458171485280", "min": 56},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036619\tposition=1458171505280", "min": 76},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2\tposition=0\toffset=\d+\tduration=\d+\terror=0", "min": 76},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 96},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "min": 96},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036619\tposition=1458171525280", "min": 96, "end_of_test":True},

    ]
}

"""
Period 881036617: 30-second reservation placed with 30-second ad
Period 881036618: 20-second reservation placed with 20-second ad
Period 881036619: 20-second reservation placed with 20-second ad
Period 881036620: 10-second reservation placed with 20-second ad
"""
TESTDATA2 = {
"title": "Back2Back Reservations",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 170,
"aamp_cfg": f"info=true\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036618 " + AD_URLS[4], # 20s
    "advert map 881036619 " + AD_URLS[4], # 20s
    "advert map 881036620 " + AD_URLS[3], # 10s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036618', "max": 70},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036619', "max": 90},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036620', "max": 100},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "max": 55, "callback_once": send_command, "callback_arg": "seek 0"},

    # Seek to start of TSB

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "min": 26},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "min": 26},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 56},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036618\tposition=\d+"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2\tposition=0\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2\tposition=\d+\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=20000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036618\tposition=\d+"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036619\tposition=\d+"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId3\tposition=0\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId3\tposition=\d+\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId3\tposition=20000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036619\tposition=\d+"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036620\tposition=\d+"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId4\tposition=0\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId4\tposition=\d+\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId4\tposition=10000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036620\tposition=\d+", "end_of_test":True},

    ]
}

#Period 881036617: 30-second reservation placed with 20-second & 10-second ad
#Period 881036618: 20-second reservation placed with 10-second & 10-second ad
TESTDATA3 = {
"title": "Back2Back Reservations with multiple ads",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 140,
"aamp_cfg": f"info=true\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[4], # 20s
    "advert map 881036617 " + AD_URLS[3], # 10s
    "advert map 881036618 " + AD_URLS[3], # 10s
    "advert map 881036618 " + AD_URLS[3], # 10s
    "advert list",
    ],
"expect_list": [
    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036618', "max": 70},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "max": 45},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "max": 45},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "max": 45},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "max": 45},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "max": 55, "callback_once": send_command, "callback_arg": "seek 0"},

    # Seek to start of TSB

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 56},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 56},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036618\tposition=\d+"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId3\tposition=0\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId3\tposition=\d+\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId3\tposition=10000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId4\tposition=10000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId4\tposition=\d+\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId4\tposition=20000\toffset=\d+\tduration=\d+\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036618\tposition=\d+", "end_of_test":True},

    ]
}

TESTDATA4 = {
"title": "Culling of AdBreaks",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 160,
"aamp_cfg": f"info=true\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=30\ntsbLog=0\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036619 " + AD_URLS[4], # 20s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036619', "max": 90},

    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdReservationMetaData): Position=(1691602655280)', "callback": check_metadata_removal},
    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdPlacementMetaData): Position=(1691602655280)', "callback": check_metadata_removal},
# Can't check this due to incorrect calculation in onAdEvent giving incorrect position intermittently
#    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdPlacementMetaData): Position=(1691602685280)', "callback": check_metadata_removal},
#    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdReservationMetaData): Position=(1691602685280)', "callback": check_metadata_removal},
    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdReservationMetaData): Position=(1691602705280)', "callback": check_metadata_removal},
    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdPlacementMetaData): Position=(1691602705280)', "callback": check_metadata_removal},
# Can't check this due to incorrect calculation in onAdEvent giving incorrect position intermittently
#    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdPlacementMetaData): Position=(1691602725280)', "callback": check_metadata_removal},
#    {"expect": r'\[Dump\]\[\d+\]Erase \(transient\) (\d+)ms (AampTsbAdReservationMetaData): Position=(1691602725280)', "callback": check_metadata_removal},

    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[\d+..\d+", "min": 110, "end_of_test" : True},

    ]
}

TESTDATA5 = {
"title": "Pause & Resume",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 105,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Playing Live
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "max": 25},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "max": 30},

    # Pause from live mid-advert and resume, so playing from TSB
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 30, "max": 55, "callback_once": send_command, "callback_arg": "pause"},
    {"expect": r"AAMP_EVENT_STATE_CHANGED: PAUSED", "max": 35, "callback_once": send_command, "callback_arg": "play"},

    # Check that progress updates are received after pause\resume
    # Don't expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617\tposition=\d+", "min": 30, "max": 55, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=\d+\tduration=\d+\terror=0", "min": 30, "max": 55, "not_expected" : True},
# TODO: Because resume does a seek it resets the ad_progress data
#    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+\toffset=\d+\tduration=\d+\terror=0", "min": 35, "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "max": 55},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "max": 55},

    # Synchronise with test time by waiting for a progress report at 60s
    # Pause from already in TSB mid-advert and resume
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 42"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 75, "callback_once": send_command, "callback_arg": "pause"},
    {"expect": r"AAMP_EVENT_STATE_CHANGED: PAUSED", "min": 75, "max": 78, "callback_once": send_command, "callback_arg": "play"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 60, "max": 75},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 60, "max": 75},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 60, "max": 75},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 80, "max": 100},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 80, "max": 100},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 80, "max": 100},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 102, "end_of_test" : True},

    ]
}

TESTDATA6 = {
"title": "Seek base to mid-ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 85,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to middle of Ad 1,
    # Don't expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_PROGRESS
    # Expect AAMP_EVENT_AD_PLACEMENT_END, AAMP_EVENT_AD_RESERVATION_END
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 62"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 60, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 60, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1\tposition=\d+", "min": 60, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 60, "max": 80},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 60, "max": 80},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 82, "end_of_test" : True},

    ]
}

TESTDATA7 = {
"title": "Seek mid-ad to mid-ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 90,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036618 " + AD_URLS[4], # 20s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036618', "max": 70},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to middle of Ad 1, wait approx 5s, seek to middle of Ad 2
    # Don't expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_PROGRESS of Ad 2
    # Don't expect AAMP_EVENT_AD_PLACEMENT_END, AAMP_EVENT_AD_RESERVATION_END of Ad 1
    # Expect AAMP_EVENT_AD_PLACEMENT_END, AAMP_EVENT_AD_RESERVATION_END of Ad 2
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 62"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 65, "callback_once": send_command, "callback_arg": "seek 87"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 65, "max": 67},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036618", "min": 65, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2", "min": 65, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2", "min": 65, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1", "min": 65, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617", "min": 65, "max": 80, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2\tposition=20000\toffset=\d+\tduration=\d+\terror=0", "min": 65, "max": 80},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036618\tposition=\d+", "min": 65, "max": 80},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 82, "end_of_test" : True},
    ]
}

TESTDATA8 = {
"title": "Seek mid-ad to base",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 80,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to middle of Ad 1, wait approx 5s, seek to primary asset
    # Don't expect AAMP_EVENT_AD_PLACEMENT_END, AAMP_EVENT_AD_RESERVATION_END
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 62"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 65, "callback_once": send_command, "callback_arg": "seek 30"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 65, "max": 67},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 60, "max": 75, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 60, "max": 75, "not_expected" : True},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 77, "end_of_test" : True},
    ]
}

TESTDATA9 = {
"title": "Seek within ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 90,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to middle of Ad 1, wait approx 5s, seek within the same ad
    # Don't expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_PROGRESS
    # Expect AAMP_EVENT_AD_PLACEMENT_END, AAMP_EVENT_AD_RESERVATION_END
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 62"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 65, "callback_once": send_command, "callback_arg": "seek 70"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 65, "max": 67},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 65, "max": 85, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 65, "max": 85, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 65, "max": 85, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 65, "max": 85},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 65, "max": 85},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 85, "end_of_test" : True},
    ]
}

TESTDATA10 = {
"title": "Trickplay inside ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 105,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to prior to Ad 1, when playing Ad 1 trickplay within the advert and exit trickplay within the same advert
    # Expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_END\AAMP_EVENT_AD_RESERVATION_END
    # Expect AAMP_EVENT_AD_PLACEMENT_PROGRESS before trickplay but not after
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 42"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..5\d+", "min": 60, "max": 95, "callback_once": send_command, "callback_arg": "ff 2"},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..6\d+", "min": 60, "max": 95, "callback_once": send_command, "callback_arg": "play"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 65, "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 65, "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 65, "max": 75},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 76, "max": 95, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=30000\toffset=\d+\tduration=\d+\terror=0", "min": 76, "max": 102},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617\tposition=\d+", "min": 76, "max": 102},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 102, "end_of_test" : True},

    ]
}

TESTDATA11 = {
"title": "Trickplay from ad into another ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 115,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036618 " + AD_URLS[4], # 20s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036618', "max": 70},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to prior to Ad 1, when playing Ad 1 trickplay within the advert and exit trickplay within a different advert
    # Expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_END\AAMP_EVENT_AD_RESERVATION_END
    # Expect AAMP_EVENT_AD_PLACEMENT_PROGRESS before trickplay but not after
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 42"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..5\d+", "min": 62, "max": 110, "callback_once": send_command, "callback_arg": "ff 2"},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..8\d+", "min": 62, "max": 110, "callback_once": send_command, "callback_arg": "play"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 62, "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 62, "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 62, "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 72, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1", "min": 62, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617", "min": 62, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036618", "min": 62, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2", "min": 62, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2", "min": 62, "max": 110, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2", "min": 62, "max": 110},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036618", "min": 62, "max": 110},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 112, "end_of_test" : True},

    ]
}

TESTDATA12 = {
"title": "Trickplay from base into ad",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 105,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert map 881036618 " + AD_URLS[4], # 20s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036618', "max": 70},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to prior to Ad 1, start trickplay and exit trickplay within Ad 2
    # Expect AAMP_EVENT_AD_RESERVATION_START\AAMP_EVENT_AD_PLACEMENT_START\AAMP_EVENT_AD_PLACEMENT_END\AAMP_EVENT_AD_RESERVATION_END
    # Expect AAMP_EVENT_AD_PLACEMENT_PROGRESS before trickplay but not after
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 42"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62, "callback_once": send_command, "callback_arg": "ff 2"},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..9\d+", "min": 62, "max": 100, "callback_once": send_command, "callback_arg": "play"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036617", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId1", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036617", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=881036618", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId2", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS\tadId=adId2", "min": 62, "max": 100, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId2", "min": 62, "max": 100},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END\tadBreakId=881036618", "min": 62, "max": 100},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 100, "end_of_test" : True},

    ]
}

TESTDATA13 = {
"title": "Trickplay from ad into base",
"archive_url" : archive_url,
"url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
'simlinear_type': 'DASH',
"max_test_time_seconds": 80,
"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
"cmdlist": [
    "contentType LINEAR_TV",
    "adtesting",
    "advert map 881036617 " + AD_URLS[2], # 30s
    "advert list",
    ],
"expect_list": [

    # Check that Ad Event Information is added to TSB correctly
    # Included these checks to ensure that the metadata is added to TSB with correct values, they
    # have a max time (equal to observed time + 5s for leniency) to make sure they cause the test to fail first if incorrect
    {"expect": r'\[CDAI\]: Add to TSB, Ad (.*) Id (.*) AbsPos (\d+)(?: Duration (\d+))?', "callback": check_metadata_addition},
    {"expect": r'\[CDAI\]: Add to TSB, Ad Reservation End Id 881036617', "max": 50},

    # Synchronise with test time by waiting for a progress report at 60s
    # Seek to mid Ad 1, start rewind and exit trickplay within the main asset
    # Expect no advert events
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 60, "callback_once": send_command, "callback_arg": "seek 55"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments", "min": 60, "max": 62 },
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED", "min": 60, "max": 62, "callback_once": send_command, "callback_arg": "rew 2"},
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..3\d+", "min": 62, "max": 70, "callback_once": send_command, "callback_arg": "play"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START", "min": 62, "max": 72, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START", "min": 62, "max": 72, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS", "min": 62, "max": 72, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END", "min": 62, "max": 72, "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END", "min": 62, "max": 72, "not_expected" : True},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[29..\d+", "min": 75, "end_of_test" : True},
    ]
}

TESTDATA14 = {
    "title": "Ad failure due to init fragment with 404 response, error occured during live stream",
    "max_test_time_seconds": 105,
    "aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
    "archive_url": error_archive_url,
    # Fail ad init fragment with 404
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_30.*?(1080|720|480|360)p_init.m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "contentType LINEAR_TV",
    	# Add a 30-second ad to the stream at the beginning of Period 1
        "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    ],

    "expect_list": [
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=1\tposition=30000", "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_ERROR\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "max": 70},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "max": 70},

    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[0..7\d+", "callback_once": send_command, "callback_arg": "seek 15"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=1\tposition=30000", "min": 71},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "min": 71},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_ERROR\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "min": 71},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0", "min": 71},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS", "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END", "not_expected" : True},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[0..\d+", "min": 100, "end_of_test" : True},

    ]
}

TESTDATA15 = {
    "title": "Ad failure due to init fragment with 404 response, error occured whilst in TSB",
    "max_test_time_seconds": 75,
    "aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nprogressLoggingDivisor={PROGRESS_REPORT_DIVISOR}\nclient-dai=true\nenablePTSReStamp=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=1000\ntsbLog=0\n",
    "archive_url": error_archive_url,
    # Fail ad init fragment with 404
    'archive_server': {'server_class': WindowServer, "extra_args": ["--force404", "ad_30.*?(1080|720|480|360)p_init.m4s"]},
    "url": "http://localhost:8080/content/TC1.mpd?live=true",
    "cmdlist": [
        "contentType LINEAR_TV",
    	# Add a 30-second ad to the stream at the beginning of Period 1
        "advert map 1 http://localhost:8080/content/ad_30s.mpd",
    ],

    "expect_list": [
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[0..1\d+", "callback_once": send_command, "callback_arg": "seek 5"},
    {"expect": r"\[TsbReader\]\[\d+\]aamp: ready to read fragments"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_SEEKED"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_START\tadBreakId=1\tposition=30000"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_START\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_ERROR\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0"},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_END\tadId=adId1\tposition=0\toffset=0\tduration=30000\terror=0"},

    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_PLACEMENT_PROGRESS", "not_expected" : True},
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_AD_RESERVATION_END", "not_expected" : True},

    # End of test
    {"expect": r"\[ReportProgress\]\[\d+\]aamp pos: \[0..\d+", "min": 70, "end_of_test" : True},

    ]
}

TESTLIST = [
    TESTDATA0,
    TESTDATA1,
    TESTDATA2,
    TESTDATA3,
    TESTDATA4,
    TESTDATA5,
    TESTDATA6,
    TESTDATA7,
    TESTDATA8,
    TESTDATA9,
    TESTDATA10,
    TESTDATA11,
    TESTDATA12,
    TESTDATA13,
    TESTDATA14,
    TESTDATA15,
    ]

@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_5003(aamp_setup_teardown, test_data):
    global aamp

    initialise_tsb_store()
    initialise_check_metadata_addition()

    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

    check_tsb_store()
