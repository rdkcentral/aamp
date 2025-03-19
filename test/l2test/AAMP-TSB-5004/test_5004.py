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

# AAMP Local TSB L2 Tests common to LLD and SLD


import os
import pytest
from inspect import getsourcefile
from l2test_pts_restamp import PtsRestampUtils

pts_restamp_utils = PtsRestampUtils()
aamp = None

archive_lld_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/ch920_10min.tgz"
LLD_URL="v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd?chunked"

archive_sld_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
SLD_URL="v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

# Test Session Manager initialization with config true
TESTDATA0 = {
	"title": "Config true",
	"logfile": "00-configtrue.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect": r"aamp_tune", "max":1},
		{"expect" : r"msg=\"File written\"", "end_of_test":True}
	]
}

# Test Session Manager not initialized with config false
TESTDATA1 = {
	"title": "Config false",
	"logfile": "01-configfalse.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=false\nprogress=true\ntsbLocation=/tmp/data\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune", "max":1},
		{"expect" : r"\[TSB Store\] Initiating with config values", "not_expected" : True},
		{"expect": r"first buffer received", "end_of_test":True}
	]
}

# Test TSB won't be created if PTS Restamp is disabled.
TESTDATA2 = {
	"title": "PTSRestamping Disabled",
	"logfile": "02-ptsrestampingdisabled.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=false\n",
	"archive_url": archive_lld_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"Local TSB is not enabled due to PTS Restamp is disabled"},
		{"expect" : r"\[TSB Store\] Initiating with config values", "not_expected" : True},
		{"expect": r"aamp_tune"},
		{"expect": r"first buffer received", "end_of_test":True}
	]
}

# Test for TSB Culling logs
TESTDATA3 = {
	"title": "Culling",
	"logfile": "03-culling.log",
	"max_test_time_seconds": 30,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=6\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_lld_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect" : r"aamp_tune", "max":1},
		{"expect" : r"TSBWrite Metrics...OK"},
		{"expect" : r"TSB Write Operation FAILED", "not_expected" : True},
		{"expect" : r"CullSegments", "min":1},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "end_of_test":True}
	]
}

# Test TSB Data Manager basic logs
TESTDATA4 = {
	"title": "Data Manager",
	"logfile": "04-datamgr.log",
	"max_test_time_seconds": 20,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\ntsbLength=4\nlldUrlKeyword=chunked\n",
	"archive_url": archive_lld_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect" : r"aamp_tune", "max":1},
		{"expect" : r"Adding Init Data:", "max":10},
		{"expect" : r"Adding fragment data:", "max":10},
		{"expect" : r"TSBWrite Metrics...OK"},
		{"expect" : r"TSB Write Operation FAILED", "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "end_of_test":True}
	]
}

# Test for TSB Store logs
TESTDATA5 = {
	"title": "TSB Library",
	"logfile": "05-tsblib.log",
	"max_test_time_seconds": 35,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=4\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect" : r"minFreePercentage : \d+", "max":1},
		{"expect" : r"msg=\"Flusher thread running\"", "max":1},
		{"expect" : r"msg=\"Flush storage content\"", "max":1},
		{"expect" : r"msg=\"Store Constructed\"", "max":1},
		{"expect" : r"msg=\"File written\" ", "max":10},
		{"expect" : r"msg=\"Deleted file\" ", "max":10},
		{"expect" : r"TSBWrite Metrics...OK", "max":10},
		{"expect" : r"TSB Write Operation FAILED", "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "end_of_test":True}
	]
}

# Read API Test
TESTDATA6 = {
	"title": "Test Read API",
	"logfile": "06-readapi.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_lld_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "progress=true\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\: ([\w:\-\. /\']+)track-audio-periodid-1729780927911-1-repid-trackId-201-tc-0-time-927982192360\.mp4', "callback_once": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Play from TSB until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[video\] Adding fragment data\: ([\w:\-\. /\']+)track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927984487213.mp4', "end_of_test": True}
	]
}

TESTDATA7 = {
	"title": "Test write to AAMP TSB",
	"logfile": "07-ptsrestamp.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_lld_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Check the PTS restamp is done correctly
		{"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},

		# Play until a specific fragment is added to AAMP TSB.
		{"expect": r'\[AddFragment\]\[\d+\]\[video\] Adding fragment data\: ([\w:\-\. /\']+)track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927981722413\.mp4', "end_of_test": True}
	]
}

TESTDATA8 = {
	"title": "VOD",
	"logfile": "08-vod.log",
	"max_test_time_seconds": 20,
	# Enable PTS restamping to ensure that AAMP Local TSB is not used for being VoD content, and not because PTS restamping is disabled.
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=4\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\nenablePTSReStamp=true\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType VOD"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "not_expected" : True},
		{"expect" : r"msg=\"File written\" ", "not_expected" : True},
		{"expect" : r"HttpRequestEnd.*track-video-periodid-.*-0-frag-881045324\.mp4", "end_of_test":True}
	]
}

TESTLIST = [
	{'testdata': TESTDATA0, 'expected_restamps': 0},
	{'testdata': TESTDATA1, 'expected_restamps': 0},
	{'testdata': TESTDATA2, 'expected_restamps': 0},
	{'testdata': TESTDATA3, 'expected_restamps': 0},
	{'testdata': TESTDATA4, 'expected_restamps': 0},
	{'testdata': TESTDATA5, 'expected_restamps': 0},
	{'testdata': TESTDATA6, 'expected_restamps': 0},
	{'testdata': TESTDATA7, 'expected_restamps': 20},
	{'testdata': TESTDATA8, 'expected_restamps': 0}
]

@pytest.fixture(params=TESTLIST)
def test_data(request):
	return request.param

def test_5004(aamp_setup_teardown, test_data):
	global aamp, pts_restamp_utils

	pts_restamp_utils.reset()
	pts_restamp_utils.max_segment_cnt = test_data.get('expected_restamps')

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data.get('testdata'))

	pts_restamp_utils.check_num_segments()
