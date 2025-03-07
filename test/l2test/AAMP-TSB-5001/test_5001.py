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

# AAMP TSB with LLD L2 Tests

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
aamp = None

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/ch920_10min.tgz"

# // Test URL with Low Latency Content. Containes the default LLD URL keyword "/low/"
LLD_URL="v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd?chunked"

SLD_URL="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

def pts_restamp_restart(match,arg):
	pts_restamp_utils.restart()

# Test Session Manager initialization with config true
TESTDATA0 = {
	"title": "Config true",
	"logfile": "00-configtrue.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},
		{"expect": r"aamp_tune"},
		{"expect" : r"msg=\"File written\"", "end_of_test":True}
	]
}

# Test Session Manager not initialized with config false
TESTDATA1 = {
	"title": "Config false",
	"logfile": "01-configfalse.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=false\ntsbLocation=/tmp/data\nsupressDecode=true\nnenablePTSReStamp=true\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune"},
		{"expect" : r"\[TSB Store\] Initiating with config values", "not_expected" : True},
		{"expect": r"first buffer received", "end_of_test":True}
	]
}

# Test TSB won't be created if PTS Restamp is disabled.
TESTDATA2 = {
	"title": "PTSRestamping Disabled",
	"logfile": "ptsrestampingdisabled2.log",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=false\n",
	"archive_url": archive_url,
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
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=6\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},
		{"expect" : r"aamp_tune"},
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
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLog=0\ntsbLength=4\nenablePTSReStamp=true\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},
		{"expect" : r"aamp_tune"},
		{"expect" : r"Adding Init Data:"},
		{"expect" : r"Adding fragment data:"},
		{"expect" : r"TSBWrite Metrics...OK"},
		{"expect" : r"TSB Write Operation FAILED", "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "end_of_test":True},
	]
}

# Test for TSB Store logs
TESTDATA5 = {
	"title": "TSB Library",
	"logfile": "05-tsblib.log",
	"max_test_time_seconds": 25,
	"aamp_cfg": "info=true\ntrace=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=4\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},
		{"expect" : r"minFreePercentage : \d+"},
		{"expect" : r"msg=\"Flusher thread running\""},
		{"expect" : r"msg=\"Flush storage content\""},
		{"expect" : r"msg=\"Store Constructed\""},
		{"expect" : r"msg=\"File written\" "},
		{"expect" : r"msg=\"Deleted file\" "},
		{"expect" : r"TSBWrite Metrics...OK"},
		{"expect" : r"TSB Write Operation FAILED", "not_expected" : True},
		{"expect" : r"Removed \d.\d+ fragment duration seconds", "end_of_test":True},
	]
}

# Test if Playback starts with LLD URL keyword '/low/' passed alongwith Non LLD URL
TESTDATA6 = {
	"title": "Non LLD + Chunked",
	"logfile": "06-nonlldchunked.log",
	"max_test_time_seconds": 20,
	"aamp_cfg": "progress=true\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=12\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"url": SLD_URL+"?chunked=/low/",
	"cmdlist": ["contentType LINEAR_TV"],
	"expect_list":
	[
		{"expect" : r"aamp_tune"},
		{"expect" : r"crashed|failure", "not_expected" : True},
		{"expect" : r"aamp pos", "end_of_test":True}
	]
}

TESTDATA7 = {
	"title": "Test Read API",
	"logfile": "07-readapi.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\: ([\w:\-\. /\']+)track-audio-periodid-1729780927911-1-repid-trackId-201-tc-0-time-927982192360\.mp4', "callback": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Play from TSB until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[video\] Adding fragment data\: ([\w:\-\. /\']+)track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927984487213.mp4', "end_of_test": True}
	]
}

TESTDATA8 = {
	"title": "Test write to AAMP TSB with PTS restamping",
	"logfile": "08-ptsrestamp.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "callback" : pts_restamp_utils.check_restamp},

		# Play until a specific fragment is added to AAMP TSB.
		{"expect": r'\[AddFragment\]\[\d+\]\[video\] Adding fragment data\: ([\w:\-\. /\']+)track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927981722413\.mp4', "end_of_test": True}
	]
}

TESTDATA9 = {
	"title": "Test Seek with PTS restamping",
	"logfile": "09-seek.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\: ([\w:\-\. /\']+)track-audio-periodid-1729780927911-1-repid-trackId-201-tc-0-time-927982192360\.mp4', "callback": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer. Restart the PTS restamp check after the Flush, to avoid a jump in the PTS values.
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"\[Flush\]\[\d+\]InterfacePlayerRDK: Pipeline is in PLAYING state position 0.000000 ret 1", "callback": pts_restamp_restart},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read from AAMP TSB.
		{"expect": r'\[ReadNext\].*?track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927984026413\.mp4', "end_of_test": True}
	]
}

TESTDATA10 = {
	"title": "Test pause on live",
	"logfile": "10-pause.log",
	"max_test_time_seconds": 35,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Pause live
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "callback": send_command, "callback_arg": "pause"},
 		# Confirm adding to TSB initially
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]',           "max": 5,},
		# Confirm adding to TSB continues (checks specific issue where fetch loop got blocked due to injection stopping).
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 30, "end_of_test": True}
	]
}

TESTDATA11 = {
	"title": "Test pause and resume",
	"logfile": "11-pause.log",
	"max_test_time_seconds": 55,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Wait 5s and then pause when the next fragment is added to TSB
		{"expect": r'\[AddFragment\]', "min": 5, "max": 9, "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 5, "max": 10},
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 5, "max": 10, "not_expected" : True},

		# Wait 10s since the start of the test and then play when the next fragment is processed
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 10, "max": 15, "callback_once": send_command, "callback_arg": "play"},
		{"expect": r'Pipeline flush seek', "min": 10, "max": 16},
		{"expect": r'AAMPGstPlayerPipeline PAUSED -> PLAYING', "min": 10, "max": 16},

		# Check the PTS restamp is done correctly after the pause and play
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "min":15, "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until 20s since the start of the test when a fragment is read from AAMP TSB, then pause
		{"expect": r'\[ReadNext\]', "min": 20, "max": 24, "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 20, "max": 25},
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 20, "max": 25, "not_expected" : True},

		# Wait 25s since start of the test and then play when the next fragment is processed
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 25, "max": 30, "callback_once": send_command, "callback_arg": "play"},
		{"expect": r'AAMPGstPlayerPipeline PAUSED -> PLAYING', "min": 25, "max": 31},
		{"expect": r'Pipeline flush seek', "min": 25, "max": 31, "not_expected" : True},

		# Play from TSB until 40s since the start of the test when a fragment is read from AAMP TSB
		{"expect": r'\[ReadNext\]', "min": 40, "end_of_test": True}
	]
}

TESTDATA12 = {
	"title": "Test trick modes (rewind and fast forward) with PTS restamping",
	"logfile": "12-trickmodes.log",
	"max_test_time_seconds": 50,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\nenablePTSReStamp=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\: ([\w:\-\. /\']+)track-audio-periodid-1729780927911-1-repid-trackId-201-tc-0-time-927982192360\.mp4', "callback": send_command, "callback_arg": "rew 2"},

		# Rewind to the beginning of the buffer
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},
		{"expect": r"GST_MESSAGE_EOS"},

		# Check the PTS restamp is done correctly
		{"expect": r'\[RestampPts\].*?\[(\w+)\] timeScale (\d+) before (\d+) after (\d+) duration (\d+) ([\w:/?\.\-]+)\r\n', "min":10, "callback" : pts_restamp_utils.check_restamp},
		# Check the PTS restamp is done correctly during trick modes (rewind and fast forward)
		{"expect": trick_modes_pts_restamp_utils.LOG_LINE, "callback" : trick_modes_pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read, then ff command will be sent.
		{"expect": r"\[ReadNext\].*?track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927986791213\.mp4", "callback": send_command, "callback_arg": "ff 2"},
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=2.000000"},

		# Fast forward to live
		{"expect": r"Adjusting position to live edge", "min":10, "end_of_test": True}
	]
}

TESTDATA13 = {
	"title": "Test pause and trick modes",
	"logfile": "13-pause.log",
	"max_test_time_seconds": 45,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"aamp_cfg": "progress=true\nprogressReportingInterval=0.250\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nenablePTSReStamp=true\n",
    "cmdlist": [ "contentType LINEAR_TV" ],
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Wait until position reaches > 50s
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..181[7-9]..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback_once": send_command, "callback_arg": "rew 2"},

		# Start rewinding
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

 		# Pause for 10s when pos is around 30s
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..180[0-4]..\d+..-?\d+..-?\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..-2.00]', "callback_once": send_command, "callback_arg": "pause"},

		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=0.000000", "min": 25, "max": 35},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 25, "max": 35, "callback_once": send_command, "callback_arg": "sleep 10000"},
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 25, "max": 35, "not_expected" : True},

		# Continue rewinding to the beginning of the buffer
		{"expect": r'sleep complete', "callback_once": send_command, "callback_arg": "rew 6"},

		# Check begining of buffer reached
		{"expect": r"GST_MESSAGE_EOS", "end_of_test": True},

		# Check the PTS restamp is done correctly once playing back from TSB
		{"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},

		# Check the PTS restamp is done correctly during trick modes (rewind and fast forward)
		{"expect": trick_modes_pts_restamp_utils.LOG_LINE, "callback" : trick_modes_pts_restamp_utils.check_restamp},
	]
}

TESTDATA = [
	{'testdata': TESTDATA0, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA1, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA2, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA3, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA4, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA5, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA6, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA7, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},

	# Verify AAMP TSB with PTS restamping
	{'testdata': TESTDATA8, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA9, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA10, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA11, 'expected_restamps': 10, 'expected_trickmodes_restamps': 0},

	# Verify AAMP TSB with PTS restamping and trick modes
	{'testdata': TESTDATA12, 'expected_restamps': 10, 'expected_trickmodes_restamps': 10},
	{'testdata': TESTDATA13, 'expected_restamps': 10, 'expected_trickmodes_restamps': 10},
]

@pytest.fixture(params=TESTDATA)
def test_data(request):
	return request.param

def test_5001(aamp_setup_teardown, test_data):
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
