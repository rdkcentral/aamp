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

# TSB L2 initial Tests


import os
import sys
import json
import copy
import pytest
from inspect import getsourcefile
from l2test_pts_restamp import PtsRestampUtils
from l2test_pts_restamp import TrickModesPtsRestampUtils

pts_restamp_utils = PtsRestampUtils()
trick_modes_pts_restamp_utils = TrickModesPtsRestampUtils()

archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

# Test URL
TEST_URL="v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

def pts_restamp_restart(match,arg):
	pts_restamp_utils.restart()

# Test Session Manager initialization with config true
TESTDATA0 = {
	"title": "configtrue",
	"logfile": "configtrue0.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_url,
	"url": TEST_URL,
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
	"title": "configfalse",
	"logfile": "configfalse1.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=false\nprogress=true\ntsbLocation=/tmp/data\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_url,
	"url": TEST_URL,
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune", "max":1},
		{"expect" : r"\[TSB Store\] Initiating TSBStore with config values", "max":3, "not_expected" : True},
		{"expect": r"first buffer received", "end_of_test":True}
	]
}

# Test for TSB Culling logs
TESTDATA2 = {
	"title": "Culling",
	"logfile": "culling2.log",
	"max_test_time_seconds": 30,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=6\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_url,
	"url": TEST_URL,
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect": r"aamp_tune", "max":1},
		{"expect" : r"TSBWrite Metrics...OK", "min":1},
		{"expect" : r"TSB Write Operation FAILED", "max":8, "not_expected" : True},
		{"expect" : r"CullSegments","min":1},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "min":8, "end_of_test":True}
	]
}

# Test TSB Data Manager basic logs
TESTDATA3 = {
	"title": "Data Manager ",
	"logfile": "datamgr3.log",
	"max_test_time_seconds": 20,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\ntsbLength=4\nlldUrlKeyword=chunked\n",
	"archive_url": archive_url,
	"url": TEST_URL,
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},
		{"expect": r"aamp_tune"," max":1},
		{"expect" : r"Adding Init Data:", "max":10},
		{"expect" : r"Adding fragment data: ", "max":10},
		{"expect" : r"TSBWrite Metrics...OK","min":3, "max":10},
		{"expect" : r"TSB Write Operation FAILED", "min":3, "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "min":8, "end_of_test":True}
	]
}

# Test for TSB Store logs
TESTDATA4 = {
	"title": "TSB Library ",
	"logfile": "tsblib4.log",
	"max_test_time_seconds": 35,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLength=4\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"archive_url": archive_url,
	"url": TEST_URL,
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
		{"expect" : r"TSB Write Operation FAILED", "max":10, "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "min":8, "end_of_test":True}
	]
}

# Read API Test
TESTDATA5 = {
	"title": "Test Read API",
	"logfile": "readapi5.log",
	"max_test_time_seconds": 60,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": "progress=true\ninfo=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nlldUrlKeyword=chunked\n",
	"expect_list":
	[
		# Wait until one specific fragment is added to TSB
		{"expect": r"\[TsbStore.cpp:\d+\] msg=\"File written\" file=\"([\w:\-\. /\']+)track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045321", "callback": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Play from TSB until one specific fragment is added to TSB
		{"expect": r'\[TsbStore.cpp:\d+\] msg=\"File written\" file=\"([\w:\-\. /\']+)track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045325', "end_of_test": True}
	]  
}

TESTDATA6 = {
	"title": "Test write to AAMP TSB with PTS restamping",
	"logfile": "WriteWithRestamp6.log",
	"max_test_time_seconds": 30,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "callback" : pts_restamp_utils.check_restamp},

		# Play until a specific fragment is added to AAMP TSB.
		{"expect": r'\[AddFragment\]\[\d+\]\[video\] Adding fragment data\:.*track-video-periodid-.*-0-frag-881045324\.mp4', "end_of_test": True}
	]
}

TESTDATA7 = {
	"title": "Test Seek with PTS restamping",
	"logfile": "seek7.log",
	"max_test_time_seconds": 30,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r"\[TsbStore.cpp:\d+\] msg=\"File written\" file=\"([\w:\-\. /\']+)track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045321", "callback": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer. Restart the PTS restamp check after the Flush, to avoid a jump in the PTS values.
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"\[Flush\]\[\d+\]InterfacePlayerRDK: Pipeline is in PLAYING state position 0.000000 ret 1", "callback": pts_restamp_restart},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read from AAMP TSB.
		{"expect": r'\[ReadNext\].*?track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045325', "end_of_test": True}
	]
}

TESTDATA8 = {
	"title": "Test pause with PTS restamping",
	"logfile": "pause8.log",
	"max_test_time_seconds": 30,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		# Wait 5s and then pause when the next fragment is added to TSB
		{"expect": r'\[AddFragment\]', "min": 5, "max": 9, "callback_once": send_command, "callback_arg": "pause"},

		# Wait 10s since the start of the test and then play when the next fragment is processed
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 10, "max": 15, "callback_once": send_command, "callback_arg": "play"},

		# Check the PTS restamp is done correctly after the pause and play
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "min":15, "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until 25s since the start of the test when a fragment is read from AAMP TSB
		{"expect": r'\[ReadNext\]', "min": 25, "end_of_test": True}
	]
}

TESTDATA9 = {
	"title": "Test trick modes (rewind and fast forward) with PTS restamping",
	"logfile": "trickmodes9.log",
	"max_test_time_seconds": 60,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		# The test data transitions over periods 881036616 and 881036617 with frag-881045317 being the first in period 881036617 
		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\:.*track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045320', "callback": send_command, "callback_arg": "rew 2"},

		# Rewind to the beginning of the buffer
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},
		{"expect": r"GST_MESSAGE_EOS"},

		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "min":15, "callback" : pts_restamp_utils.check_restamp},
		# Check the PTS restamp is done correctly during trick modes (rewind and fast forward)
		{"expect": trick_modes_pts_restamp_utils.LOG_LINE, "callback" : trick_modes_pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read, then ff command will be sent.
		{"expect": r"\[ReadNext\].*?track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045321", "callback": send_command, "callback_arg": "ff 2"},
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=2.000000"},

		# Fast forward to live
		{"expect": r"Adjusting position to live edge","min":10, "end_of_test": True}
	]
}


TESTDATA = [
	# Verify AAMP TSB without PTS restamping
	{'testdata': TESTDATA0, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA1, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA2, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA3, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA4, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA5, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},

	# Verify AAMP TSB with PTS restamping
	{'testdata': TESTDATA6, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA7, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA8, 'expected_restamps': 8, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA9, 'expected_restamps': 8, 'expected_trickmodes_restamps': 10}
]


@pytest.fixture(params=TESTDATA)
def test_data(request):
	return request.param

def test_5002(aamp_setup_teardown, test_data):
	global aamp, pts_restamp_utils, trick_modes_pts_restamp_utils

	pts_restamp_utils.reset()
	pts_restamp_utils.max_segment_cnt = test_data.get('expected_restamps')

	trick_modes_pts_restamp_utils.reset()
	trick_modes_pts_restamp_utils.max_segment_cnt = test_data.get('expected_trickmodes_restamps')

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data.get('testdata'))

	pts_restamp_utils.check_num_segments()
	trick_modes_pts_restamp_utils.check_num_segments()
