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

# AAMP Local TSB LLD L2 Tests


import os
import pytest
from inspect import getsourcefile
from l2test_pts_restamp import PtsRestampUtils
from l2test_pts_restamp import TrickModesPtsRestampUtils
from l2test_aamp_tsb import AampTsbUtils

# The progress report is printed in the log at the interval times 4, i.e. 1s
PROGRESS_REPORT_INTERVAL = 0.250
PROGRESS_REPORT_DIVISOR = 4
PROGRESS_REPORT_INTERVAL_IN_LOG = PROGRESS_REPORT_INTERVAL * PROGRESS_REPORT_DIVISOR
PROGRESS_REPORT_TOLERANCE = 1

POSITION_TOLERANCE_RW_SEEK = 1.5
SEEK_POSITION = 6

pts_restamp_utils = PtsRestampUtils()
trick_modes_pts_restamp_utils = TrickModesPtsRestampUtils()
aamp_tsb_utils = AampTsbUtils()
aamp = None
position = 0
state = None
tsb_start_position = 0

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/misc/ch920_10min.tgz"

LLD_URL="v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd?chunked"

SLD_URL="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)

def send_seek_command(match, arg):
	aamp.sendline(f"seek {aamp_tsb_utils.seek_position+aamp_tsb_utils.first_progress_event_position}")

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

def update_tsb_start(match, arg):
    global tsb_start_position
    tsb_start_position = float(match.group(1))

def verify_autoplay_pos(match, arg):
    tsb_read_position = float(match.group(1))
    assert tsb_start_position == tsb_read_position, f"tsb_read_position {tsb_read_position} is not equal to tsb_start_position {tsb_start_position}"

TESTDATA0 = {
	"title": "Test Seek",
	"logfile": "00-seek.log",
	"max_test_time_seconds": 20,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
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
		{"expect": pts_restamp_utils.LOG_LINE, "callback" : pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read from AAMP TSB.
		{"expect": r'\[ReadNext\].*?track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927984026413\.mp4', "end_of_test": True}
	]
}

TESTDATA1 = {
	"title": "Test pause on live",
	"logfile": "01-pause-live.log",
	"max_test_time_seconds": 25,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": f"info=true\ntrace=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

 		# Confirm caching chunks initially
		{"expect": r'\[CacheFragmentChunk\]\[\d+\]', "max": 5},

 		# Confirm adding to TSB initially
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "max": 5},

		# Wait for 5s then Pause
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 5, "max": 8, "callback_once": send_command, "callback_arg": "pause"},
		{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED'},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback": check_position, "callback_arg": "playing"},
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..0.00]', "callback": check_position, "callback_arg": "paused"},

		# Confirm chunks not cached after paused
		# And check the fetcher loop continues to add fragments to the TSB
		{"expect": r'\[CacheFragmentChunk\]\[\d+\]', "min": 9, "not_expected" : True},
		{"expect": r'\[AddFragment\]\[\d+\]\[video\]', "min": 20, "end_of_test": True}
	]
}

TESTDATA2 = {
	"title": "Test pause and resume",
	"logfile": "02-pause-resume.log",
	"max_test_time_seconds": 55,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
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
		{"expect": pts_restamp_utils.LOG_LINE, "min":15, "callback" : pts_restamp_utils.check_restamp},

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
	"max_test_time_seconds": 50,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\nsupressDecode=true\n",
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
		{"expect": pts_restamp_utils.LOG_LINE, "min":10, "max":30, "callback" : pts_restamp_utils.check_restamp},
		# Check the PTS restamp is done correctly during trick modes (rewind and fast forward)
		{"expect": trick_modes_pts_restamp_utils.LOG_LINE, "callback" : trick_modes_pts_restamp_utils.check_restamp},

		# Play from TSB until a specific fragment is read, then ff command will be sent.
		{"expect": r"\[ReadNext\].*?track-video-periodid-1729780927911-1-repid-trackId-102-tc-0-time-927986791213\.mp4", "callback": send_command, "callback_arg": "ff 2"},
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=2.000000"},

		# Fast forward to live, check that aamp-cli receives the ENTERING_LIVE event
		{"expect": r"AAMP_EVENT_ENTERING_LIVE", "min":10, "end_of_test": True}
	]
}

TESTDATA4 = {
	"title": "Test pause and trick modes",
	"logfile": "04-pause-trickmodes.log",
	"max_test_time_seconds": 45,
	'simlinear_type': 'DASH',
	"archive_url": archive_url,
	"url": LLD_URL,
	"aamp_cfg": f"progress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\n",
    "cmdlist": [ "contentType LINEAR_TV" ],
	"expect_list":
	[
		{"expect" : r"\[TSB Store\] Initiating with config values"},

		# Initial pos 1795
		# Wait until position reaches >= 1817 (approx 22s)
		{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..181[7-9]..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "callback_once": send_command, "callback_arg": "rew 2"},

		# Start rewinding
		{"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
		{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

 		# Pause for 10s when pos is around 1804
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

TESTDATA5 = {
    "title": "Test culled TSB - autoplay immediate",
    "logfile": "05-autoplay-immediate.log",
    "max_test_time_seconds": 50,
    'simlinear_type': 'DASH',
    "archive_url": archive_url,
    "url": LLD_URL,
    "aamp_cfg": f"info=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=30\ntsbLog=0\nlivePauseBehavior=0\n",
    "cmdlist": [ "contentType LINEAR_TV" ],
    "expect_list":
    [
        {"expect" : r"\[TSB Store\] Initiating with config values", "max":1},

        # Wait for 5s then Pause
        {"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 5, "max": 8, "callback_once": send_command, "callback_arg": "pause"},
        {"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "min": 5, "max": 8},
        {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=0.000000", "min": 5, "max": 8},
        {"expect": r'AAMPGstPlayerPipeline \w+ -> PLAYING', "min": 8, "max": 25, "not_expected" : True},

        # When culling starts, store the tsb start position
        {"expect": r'\[UpdateCullingState\]\[\d+\]Resume playback since playlist start position\((\d+.\d+)\) has moved past paused position\(\d+.\d+\)', "min": 26, "max": 38, "callback_once": update_tsb_start},
        # Check that the autoplay is happening with an expected position
        {"expect": r'\[ReadNext\]\[\d+\]\[video\] Returning fragment: absPos (\d+.\d+)s.*?', "min": 26, "max": 38, "callback_once": verify_autoplay_pos},
        {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=1.000000", "min": 26, "max": 38},
        {"expect": r'AAMPGstPlayerPipeline PAUSED -> PLAYING', "min": 26, "max": 38},
        # Continue playing from TSB until 38s since the start of the test when UpdateProgress is logged from AAMP TSB
        {"expect": r'\[UpdateProgress\]\[\d+\]tsb pos: \[(\d+.\d+)..\[X\]..\d+.\d+\]', "min": 38, "end_of_test": True},
    ]
}

# Given that presentation is in progress at any fast-forward rate
# The Player presents video I-frames from the linear content in fast forward at the specified rate
# When presentation position reaches the TSB End position
# Then the player changes playback rate to X 1 from the current position and notifies the client of a speed change event
def generate_ff_testdata(ff_data):
	test_data = {
		"title": f"Acceptance Criteria for forward trick play ff {ff_data}",
		"logfile": f"ff{ff_data}-trickmodes.log",
		"max_test_time_seconds": 80,
		"simlinear_type": "DASH",
		"archive_url": archive_url,
		"url": LLD_URL,
		"cmdlist": ["contentType LINEAR_TV"],
		"aamp_cfg": f"trace=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\n",
		"expect_list": [
			# Wait for playing and then pause
			{"expect": r"AAMP_EVENT_STATE_CHANGED: PLAYING", "callback_once": send_command, "callback_arg": "pause"},
			# Pause for 30 seconds for speed of 2, 60 seconds for speed of 64
			{"expect": r'AAMPGstPlayerPipeline PLAYING -> PAUSED', "callback_once": send_command, "callback_arg": f"sleep {30000 if ff_data == 2 else 60000}"},
			# Could have used ReportProgress to get log time stamp but GetPositionMilliseconds is accurate
			{"expect": r"(\d+\.\d+):.*?\[GetPositionMilliseconds\]\[\d+\]Returning Position as", "min": 25 if ff_data == 2 else 55, "callback": aamp_tsb_utils.get_time_stamp_at_trick_play},
			{"expect": r'sleep complete', "callback_once": send_command, "callback_arg": f"ff {ff_data}"},
			{"expect": rf"AAMP_EVENT_SPEED_CHANGED current rate={ff_data}.000000", "callback_once": aamp_tsb_utils.set_flag_time_stamp_at_trick_play_achieved},

			{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=1.000000", "min":45},
			{"expect": r'(\d+\.\d+).*?\[ReportProgress\]\[\d+\]aamp pos: \[(\d+)..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 45, "callback": aamp_tsb_utils.check_current_play_position, "callback_arg": ("ff",ff_data)},

			# Checks the position in the playing state
			{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min":45, "callback": check_position, "callback_arg": "playing"},
			# Carry on additional verification (well past the assertion criteria) to terminate the test successfully
			{"expect": r"\[GetPositionMilliseconds\]", "min":60 if ff_data == 2 else 70, "end_of_test": True}
		],
	}
	return test_data

# Given that presentation is in progress at any negative rate
# The Player presents video I-frames from the linear content in reverse at the specified rate
# When the presentation position reaches the TSB start position
# Then The player changes playback rate to x 1 from the current position and notifies the client of a speed change event
def generate_rew_testdata(rew_data):
	test_data = {
		"title": f"Acceptance Criteria for rewind rew {rew_data}",
		"logfile": f"rew{rew_data}-trickmodes.log",
		"max_test_time_seconds": 80,
		'simlinear_type': 'DASH',
		"archive_url": archive_url,
		"url": LLD_URL,
		"cmdlist": ["contentType LINEAR_TV"],
		"aamp_cfg": f"trace=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\n",
		"expect_list": [
			# Wait until a minimum of 10s data for 2x and 60s data for 64x is built into buffer
			{"expect": r'\[ReportProgress\]\[\d+\]', "min": 10 if rew_data == 2 else 60, "callback_once": send_command, "callback_arg": f"rew {rew_data}"},
			{"expect": rf"AAMP_EVENT_SPEED_CHANGED current rate=-{rew_data}.000000"},
			{"expect": r"GST_MESSAGE_EOS", "min": 10 if rew_data == 2 else 60},
			# Once the speed changes to X 1 get the play position from the next sample.
			{"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=1.000000"},
			{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[(\d+)..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 11 if rew_data == 2 else 61, "callback_once": aamp_tsb_utils.check_current_play_position, "callback_arg": "rew"},

			# Checks the position in the playing state
			{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 14 if rew_data == 2 else 62, "callback": check_position, "callback_arg": "playing"},
			# Carry on additional verification (well past the assertion criteria) to terminate the test successfully
			{"expect": r"\[GetPositionMilliseconds\]", "min": 20 if rew_data == 2 else 70, "end_of_test": True}
		]
	}
	return test_data

# Given that presentation is in progress from the TSB
# Seek to a specified position within the TSB depth
# The Player presents the audio and video content from the TSB at x1 rate from the seeked position
def generate_seek_testdata(extra_config):
	test_data = {
		"title": f"AC for seek {extra_config}",
		"logfile": f"seek-trickmode-{extra_config}.log",
		"max_test_time_seconds": 40,
		"simlinear_type": "DASH",
		"archive_url": archive_url,
		"url": LLD_URL,
		"cmdlist": ["contentType LINEAR_TV"],
		"aamp_cfg": f"trace=true\nprogress=true\nprogressReportingInterval={PROGRESS_REPORT_INTERVAL}\ninfo=true\nlocalTSBEnabled=true\ntsbLocation=/tmp/data\ntsbLength=500\ntsbLog=0\n{extra_config}\n",

		"expect_list": [
			{"expect": r'\[ReportProgress\]\[\d+\]Send first progress event with position (\d+)', "callback_once": aamp_tsb_utils.extract_first_progress_event_position },

			# Wait 10s and then pause when the next fragment is added to TSB
			{"expect": r'\[ReportProgress\]\[\d+\]', "min": 10, "max": 14, "callback_once": send_command, "callback_arg": "pause"},
			# Wait 15s since the start of the test and then play when the next fragment is processed
			{"expect": r'\[ReportProgress\]\[\d+\]', "min": 15,"max": 20, "callback_once": send_command, "callback_arg": "play"},

			{"expect": r'\[ReportProgress\]\[\d+\]', "min":20, "max":25, "callback_once": send_seek_command},
			{"expect": r'aamp_Seek\(\d+.\d+\)', "min":20},
			{"expect": r'\[(SeekInternal)\]\[.*\]aamp_Seek position adjusted to absolute value: ', "callback" : aamp_tsb_utils.set_flag_check_seek_position},

			{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[(\d+)..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min": 20,  "callback": aamp_tsb_utils.check_current_play_position,"callback_arg": "seek"},

			# Checks the position in the playing state
			{"expect": r'\[ReportProgress\]\[\d+\]aamp pos: \[\d+..(\d+)..\d+..-?\d+..\d+.\d+..-?\d+.\d+..\w*..\d+..\d+..1.00]', "min":24, "callback": check_position, "callback_arg": "playing"},
			{"expect": r"\[GetPositionMilliseconds\]\[\d+]rate=1.000000.", "min":30, "end_of_test": True}
		]
	}
	return test_data

TESTDATA_FF2 = generate_ff_testdata(2)
TESTDATA_FF64 = generate_ff_testdata(64)
TESTDATA_REW2 = generate_rew_testdata(2)
TESTDATA_REW64 = generate_rew_testdata(64)
TESTDATA_SEEK = generate_seek_testdata("")
TESTDATA_SEEK_ABS = generate_seek_testdata("useAbsoluteTimeline=true")

TESTLIST = [
	{'testdata': TESTDATA0, 'expected_restamps': 20, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA1, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA2, 'expected_restamps': 10, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA3, 'expected_restamps': 10, 'expected_trickmodes_restamps': 10},
	{'testdata': TESTDATA4, 'expected_restamps': 10, 'expected_trickmodes_restamps': 10},
	{'testdata': TESTDATA5, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	# fast forward show position jump in transition from trick mode speed to x1 i.e. live
	# The bug is investigated under VPLAY-9019. Once fixed FF2, FF64 can be enabled
	#{'testdata': TESTDATA_FF2, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	#{'testdata': TESTDATA_FF64, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA_REW2, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA_REW64, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA_SEEK, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0},
	{'testdata': TESTDATA_SEEK_ABS, 'expected_restamps': 0, 'expected_trickmodes_restamps': 0}
]

@pytest.fixture(params=TESTLIST)
def test_data(request):
	return request.param

def test_5001(aamp_setup_teardown, test_data):
	global aamp, pts_restamp_utils, trick_modes_pts_restamp_utils, state, aamp_tsb_utils, POSITION_TOLERANCE_RW_SEEK, SEEK_POSITION

	# Start each test with a clean state
	state = None

	pts_restamp_utils.reset()
	pts_restamp_utils.max_segment_cnt = test_data.get('expected_restamps')

	trick_modes_pts_restamp_utils.reset()
	trick_modes_pts_restamp_utils.max_segment_cnt = test_data.get('expected_trickmodes_restamps')

	aamp_tsb_utils.POSITION_TOLERANCE_RW_SEEK = POSITION_TOLERANCE_RW_SEEK
	aamp_tsb_utils.seek_position = SEEK_POSITION

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data.get('testdata'))

	pts_restamp_utils.check_num_segments()
	trick_modes_pts_restamp_utils.check_num_segments()
