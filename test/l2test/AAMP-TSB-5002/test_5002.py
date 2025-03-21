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

# AAMP Local TSB SLD L2 Tests


import os
import pytest
from inspect import getsourcefile
from l2test_pts_restamp import PtsRestampUtils
from l2test_pts_restamp import TrickModesPtsRestampUtils

# The progress report is printed in the log at the interval times 4, i.e. 1s
PROGRESS_REPORT_INTERVAL = 0.250
PROGRESS_REPORT_DIVISOR = 4
PROGRESS_REPORT_INTERVAL_IN_LOG = PROGRESS_REPORT_INTERVAL * PROGRESS_REPORT_DIVISOR
PROGRESS_REPORT_TOLERANCE = 1

pts_restamp_utils = PtsRestampUtils()
trick_modes_pts_restamp_utils = TrickModesPtsRestampUtils()
aamp = None
position = 0
state = None

archive_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"

TEST_URL="v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

def pts_restamp_restart(match, arg):
	pts_restamp_utils.restart()

def check_position(match, new_state):
	global position, state
	new_position = int(match.group(1))
	position_diff = new_position - position
	min_expected_position = PROGRESS_REPORT_INTERVAL_IN_LOG - PROGRESS_REPORT_TOLERANCE
	max_expected_position = PROGRESS_REPORT_INTERVAL_IN_LOG + PROGRESS_REPORT_TOLERANCE

	if state == "paused" and new_state == "paused":
		assert position_diff == 0, f"Paused: previous position {position}, new {new_position}, diff {position_diff} != 0s"
	elif state == "playing" and new_state == "paused":
		assert abs(position_diff) <= max_expected_position, f"Playing->Paused: previous position {position}, new {new_position}, diff {position_diff} > {max_expected_position}s"
	elif state == "paused" and new_state == "playing":
		assert abs(position_diff) <= max_expected_position, f"Paused->Playing: previous position {position}, new {new_position}, diff {position_diff} > {max_expected_position}s"
	elif state == "playing" and new_state == "playing":
		assert position_diff >= min_expected_position, f"Playing: previous position {position}, new {new_position}, diff {position_diff} < {min_expected_position}s"
		assert position_diff <= max_expected_position, f"Playing: previous position {position}, new {new_position}, diff {position_diff} > {max_expected_position}s"

	position = new_position
	state = new_state

TESTDATA0 = {
	"title": "Test Seek",
	"logfile": "00-seek.log",
	"max_test_time_seconds": 30,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r"\[TsbStore.cpp:\d+\] msg=\"File written\" file=\"([\w:\-\. /\']+)track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045321", "callback": send_command, "callback_arg": "seek 0"},

		# Seek to the beginning of the buffer. Restart the PTS restamp check after the Flush, to avoid a jump in the PTS values.
		{"expect": r"aamp_Seek\(0.000000\)"},
		{"expect": r"\[Flush\]\[\d+\]InterfacePlayerRDK: Pipeline is in PLAYING state position 0.000000 ret 1", "callback": pts_restamp_restart},
		{"expect": r"msg=\"Got size\""},
		{"expect": r"File Read"},

		# Check the PTS restamp is done correctly
		{"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read from AAMP TSB.
		{"expect": r'\[ReadNext\].*?track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045325', "end_of_test": True}
	]
}

TESTDATA1 = {
	"title": "Test pause on live",
	"logfile": "01-pause-live.log",
	"max_test_time_seconds": 35,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Pause live
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED'},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback": check_position, "callback_arg": "playing"},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..0.00]', "callback": check_position, "callback_arg": "paused"},
 		# Confirm adding to TSB initially
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "max": 5,},
		# Confirm chunks not injected but adding to TSB continues
		# Specifically checking the fetch loop has not got blocked due to injection.
		{"expect": r'\[CacheFragment\]\[\d+\]\[video\][\w\- ]+ not injecting'},
		{"expect": r'\[CacheFragment\]\[\d+\]\[audio\][\w\- ]+ not injecting'},
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 30, "end_of_test": True}
	]
}

TESTDATA2 = {
	"title": "Test pause and resume",
	"logfile": "02-pause-resume.log",
	"max_test_time_seconds": 55,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Wait 5s and then pause when the next fragment is added to TSB
		{"expect": r'\[AddFragment\]', "min": 5, "max": 9, "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 5, "max": 10},
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 5, "max": 10, "not_expected" : True},

		# Wait 10s since the start of the test and then play when the next fragment is processed
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 10, "max": 15, "callback_once": send_command, "callback_arg": "play"},
		{"expect": r'Pipeline flush seek', "min": 10, "max": 16},
		{"expect": r'AAMPGstPlayerPipeline PAUSED -> PLAYING', "min": 10, "max": 16},

		# Check the PTS restamp is done correctly after the pause and play
		{"expect": pts_restamp_utils.LOG_LINE, "min":16, "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until 20s since the start of the test when a fragment is read from AAMP TSB, then pause
		{"expect": r'\[ReadNext\]', "min": 20, "max": 24, "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 20, "max": 25},
		{"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 20, "max": 25, "not_expected" : True},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback": check_position, "callback_arg": "playing"},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..0.00]', "callback": check_position, "callback_arg": "paused"},

		# Wait 25s since start of the test and then play when the next fragment is processed
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 25, "max": 30, "callback_once": send_command, "callback_arg": "play"},
		{"expect": r'AAMPGstPlayerPipeline PAUSED -> PLAYING', "min": 25, "max": 31},
		{"expect": r'Pipeline flush seek', "min": 25, "max": 31, "not_expected" : True},

		# Play from TSB until 40s since the start of the test when a fragment is read from AAMP TSB
		{"expect": r'\[ReadNext\]', "min": 40, "end_of_test": True}
	]
}

TESTDATA3 = {
	"title": "Test trick modes (rewind and fast forward)",
	"logfile": "03-trickmodes.log",
	"max_test_time_seconds": 60,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# The test data transitions over periods 881036616 and 881036617 with frag-881045317 being the first in period 881036617
		# Wait until one specific fragment is added to TSB (~10s)
		{"expect": r'\[AddFragment\]\[\d+\]\[audio\] Adding fragment data\:.*track-audio-periodid-881036617-([\w:\-\. /\']+)frag-881045320', "callback": send_command, "callback_arg": "rew 2"},

		# Rewind to the beginning of the buffer
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},
		{"expect": r"GST_MESSAGE_EOS"},

		# Check the PTS restamp is done correctly
		{"expect": pts_restamp_utils.LOG_LINE, "min":15, "callback" : pts_restamp_utils.check_restamp},
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

TESTDATA4 = {
	"title": "Test pause and trick modes",
	"logfile": "04-pause-trickmodes.log",
	"max_test_time_seconds": 45,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": TEST_URL,
	"aamp_cfg": f"progress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\n",
    "cmdlist": [ "contentType LINEAR_TV" ],
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

		# Initial position 29s
		# Wait until position reaches >= 50 (approx 21s)
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..5\d..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback_once": send_command, "callback_arg": "rew 2"},

		# Start rewinding
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

 		# Pause for 10s when pos is around 30s
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..3\d..\d+..-?\d+..-?\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..-2.00]', "callback_once": send_command, "callback_arg": "pause"},

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

TESTLIST = [
	{'testdata': TESTDATA0, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA1, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA2, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA3, 'expected_restamps': 8, 'expected_trickmodes_restamps': 10},
	{'testdata': TESTDATA4, 'expected_restamps': 10, 'expected_trickmodes_restamps': 10}
]

@pytest.fixture(params=TESTLIST)
def test_data(request):
	return request.param

def test_5002(aamp_setup_teardown, test_data):
	global aamp, pts_restamp_utils, trick_modes_pts_restamp_utils, state

	# Start each test with a clean state
	state = None

	pts_restamp_utils.reset()
	pts_restamp_utils.max_segment_cnt = test_data.get('expected_restamps')

	trick_modes_pts_restamp_utils.reset()
	trick_modes_pts_restamp_utils.max_segment_cnt = test_data.get('expected_trickmodes_restamps')

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data.get('testdata'))

	pts_restamp_utils.check_num_segments()
	trick_modes_pts_restamp_utils.check_num_segments()
