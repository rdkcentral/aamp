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

# Starts simlinear, a web server for serving ABR test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
#
# Also see README.txt

from inspect import getsourcefile
import os
import pytest
import re

###############################################################################
archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/TST_2004_HOTCDVR/l2_hotcdvr_working.tar.gz"
TESTDATA1 = {
"title": "Check for aamp pos value",
"archive_url": archive_url,
"url": "l2_hotcdvr_working/manifest.mpd",
"simlinear_type": "DASH",
"max_test_time_seconds": 40,
"aamp_cfg": "info=true\nprogress=true\njsinfo=true\nenableSeekableRange=true\n",
"cmdlist": [
  "seek 1250",
    ],
"expect_list": [
    #Check to ensure start(1st parameter) is zero and also the end position(third para.) increases with time
    {"expect": r"aamp pos: \[(.*)" + re.escape("0..1253..1257..") + r"(.*)]", "max": 20},
    {"expect": r"aamp pos: \[(.*)" + re.escape("0..1265..1269..") + r"(.*)]","min":20,"max":25},
    #Check to ensure start(1st parameter) is zero and also the end position(third parameter) is same for cold cdvr
    {"expect": r"\[AAMPCLI\] AAMP_EVENT_MANIFEST_REFRESH_NOTIFY.*manifestType\[static\]"},
    {"expect": r"aamp pos: \[(.*)" + re.escape("0..1269..1281..") + r"(.*)]","min":25,"max":30},
    {"expect": r"aamp pos: \[(.*)" + re.escape("0..1273..1281..") + r"(.*)]","min":30,"max":40,"end_of_test":True}
    
]
}




#The full list of tests
TESTLIST = [TESTDATA1]


############################################################


"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param


def test_2004(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

