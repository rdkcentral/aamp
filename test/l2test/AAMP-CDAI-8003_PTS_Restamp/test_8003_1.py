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


from inspect import getsourcefile
from datetime import datetime
import os
import pytest


# Positions are from the start of the asset
TEST_SPEEDS = [
	{'position_secs' : 0,	'direction': "ff",	'rate': 2 },
	{'position_secs' : 0,	'direction': "ff",	'rate': 6 },
	{'position_secs' : 0,	'direction': "ff",	'rate': 12 },
	{'position_secs' : 0,	'direction': "ff",	'rate': 30 },
	{'position_secs' : 300,	'direction': "rew",	'rate': -2 },
	{'position_secs' : 300,	'direction': "rew",	'rate': -6 },
	{'position_secs' : 300,	'direction': "rew",	'rate': -12 },
	{'position_secs' : 300,	'direction': "rew",	'rate': -30 },
]

BASE_POSITION_TOLERANCE_MS = 100

current_speed = 0
speed_change_time = datetime.now()


def set_speed_change_time(match):
	global speed_change_time, current_speed
	reported_speed = int(match.group(1))
	print(f"Speed: actual {reported_speed}, expected {current_speed}")
	assert reported_speed == current_speed
	speed_change_time = datetime.now()

def verify_position(match):
	global speed_change_time, current_speed
	time_delta = datetime.now() - speed_change_time
	time_delta_ms = (time_delta.seconds * 1000) + (time_delta.microseconds / 1000)
	asset_position_ms = int(match.group(1))
	expected_position_ms = time_delta_ms * current_speed
	# Scale the position tolerance according to the current speed
	position_tolerance_ms = BASE_POSITION_TOLERANCE_MS * abs(current_speed)
	print(f"Position: actual {asset_position_ms}, expected {expected_position_ms:.2f}, " +
		  f"diff {(asset_position_ms - expected_position_ms):.2f}, tol {position_tolerance_ms}")
	assert asset_position_ms >= (expected_position_ms - position_tolerance_ms)
	assert asset_position_ms <= (expected_position_ms + position_tolerance_ms)

############################################################

@pytest.fixture(params=TEST_SPEEDS)
def test_data(request):
	return request.param

def test_8003_1(httpserver_setup_teardown, aamp_setup_teardown, test_data):
	global current_speed
	aamp = aamp_setup_teardown
	current_speed = test_data['rate']
	server_address = httpserver_setup_teardown
	this_test = {
		"title": "Test Dash PTS Trickmode Restamping",
		"max_test_time_seconds": 30,
		"aamp_cfg": "info=true\ndebug=true\nprogress=true\nenablePTSReStamp=true\nprogressReportingInterval=1.0\n",
		"expect_list": [
			# Manifest is hosted locally - see comments in main.mpd for details
			{"cmd": f"http://{server_address[0]}:{server_address[1]}/AAMP-CDAI-8003_PTS_Restamp/testdata/main.mpd"},
			{"expect": "AAMP_EVENT_TUNED"},

			{"cmd": f"seek {test_data['position_secs']}"},
			{"expect":"AAMP_EVENT_STATE_CHANGED: SEEKING"},
			{"expect":"AAMP_EVENT_STATE_CHANGED: PLAYING"},

			{"cmd": f"{test_data['direction']} {abs(current_speed)}"},
			{"expect":r'AAMP_EVENT_SPEED_CHANGED current rate=(-?\d+)',
			"callback" : set_speed_change_time},

			# Ignore first position updates following speed change to allow streaming to settle down
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)'},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)'},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)'},

			# Verify position relative to time of speed change for the next 5 seconds
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)', "callback" : verify_position},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)', "callback" : verify_position},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)', "callback" : verify_position},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)', "callback" : verify_position},
			{"expect": r'GetPositionMilliseconds.*?rc - (-?\d+)', "callback" : verify_position},

			{"cmd": "stop"},
			{"expect": "aamp_stop PlayerState"},
		]
	}
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_a(this_test)
