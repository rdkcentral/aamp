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

# AAMP LLD L2 Tests

import os
import sys
import json
import copy
import pytest
import time

from inspect import getsourcefile

aamp = None

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/ch920_10min.tgz"

# // Test URL with Low Latency Content. 
LLD_URL="v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd?chunked"

first_pos_reported = 0
first_pos_time = 0

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

def save_first_pos_reported(match,arg):
	global first_pos_reported, first_pos_time
	first_pos_reported = int(match.group(1))
	first_pos_time = time.time()
	str = (f"first_pos_reported: {first_pos_reported} time: {first_pos_time:.2f}")
	print(str)

def check_pos_reported(match,arg):
	global first_pos_reported, first_pos_time
	pos_reported = int(match.group(1))
	elapsed = time.time() - first_pos_time
	str = (f"first_pos_reported: {first_pos_reported} pos_reported: {pos_reported} elapsed: {elapsed:.2f}")
	print(str)
	assert (pos_reported >= first_pos_reported + int(elapsed)) and (pos_reported <= first_pos_reported + int(elapsed) + 1), str


# Test Live LLD stream
TESTDATA0 = {
	"title": "LLD Playback With Position Verification",
	"logfile": "00-lld_pos.log",
	"max_test_time_seconds": 20,
	"aamp_cfg": "info=true\nprogress=true\nprogressReportingInterval=0.250\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r'\[ReportProgress\]\[\d+\]Send first progress event with position (\d+)', "callback_once": save_first_pos_reported},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback": check_pos_reported},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..\d+..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 18, "end_of_test": True},
	]
}

# Test NotifyBitrate for LLD with localTSBEnabled false
TESTDATA1 = {
	"title": "LLD playback with no AAMP local TSB notifybitrate Verification",
	"logfile": "01-notifybitrate_without_tsb.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=false\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"aamp_tune"},
		{"expect" : r"\[TSB Store\] Initiating with config values", "not_expected" : True},
		{"expect": r"NotifyBitRateChangeEvent"},
		{"expect": r"IP_AAMP_TUNETIME", "end_of_test":True}
	]
}

# Test NotifyBitrate for LLD with localTSBEnabled true
TESTDATA2 = {
	"title": "LLD playback with AAMP local TSB notifybitrate Verification",
	"logfile": "02-notifybitrate_with_tsb.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"aamp_tune"},
		{"expect" : r"\[TSB Store\] Initiating with config values"},
		{"expect": r"NotifyBitRateChangeEvent"},
		{"expect": r"IP_AAMP_TUNETIME", "end_of_test":True}
	]
}

TESTDATA = [
	{'testdata': TESTDATA0},
	{'testdata': TESTDATA1},
	{'testdata': TESTDATA2},
]

@pytest.fixture(params=TESTDATA)
def test_data(request):
	return request.param

def test_1015(aamp_setup_teardown, test_data):
	global aamp

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data.get('testdata'))

	
