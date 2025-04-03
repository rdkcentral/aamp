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


import os
import pytest
from inspect import getsourcefile

aamp = None


archive_sld_url = "https://cpetestutility.stb.r53.xcal.tv/AAMP/simlinear/SkyAtlantic/30t-2/skyatlantic-30t-2.tgz"
SLD_URL="v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd"

# Callbacks used by the tests
def send_command(match, command):
	aamp.sendline(command)


sap_track = '{"languages":["eng"],"rendition":"alternate","accessibility":{"scheme":"urn:mpeg:dash:role:2011","string_value":"description"}}'
unavailable_track = '{"languages":["spa"]}'

"""
Test that we can switch to an alternative audio track with AAMP TSB enabled
"""
TESTDATA0 = {
	"title": "Audio Track Switching when watching live",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune", "max":1},

		#Make sure we stay in the right 'mode'
		{"expect": r"Local AAMP TSB 0", "not_expected": True },
		{"expect": r"Local AAMP TSB 1" },
		#Check we are fetching from 'std' audio track and switch to alternative
		{"expect": r"CacheFragment.*track-audio-periodid-881036616", "callback_once":send_command, "callback_arg": f"set preferredLanguages {sap_track}"},

		#Check that the TSB Store is being flushed
		{"expect": r"TsbStore.*msg=\"Flush triggered\" oldActiveDirNum=\"2\""},

		#Check we are fetching from alternate audio track and finish test
		{"expect": r"CacheFragment.*track-sap-periodid-881036617", "end_of_test":True}
	]
}

"""
Test that we can switch to an alternative audio track when playing from AAMP TSB
"""
TESTDATA1 = {
	"title": "Audio Track Switching when playing from AAMP TSB",
	"max_test_time_seconds": 25,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune", "max":1},

		#Make sure we stay in the right 'mode'
		{"expect": r"Local AAMP TSB 0", "not_expected": True },
		{"expect": r"Local AAMP TSB 1" },
		#Check we are fetching from 'std' audio track and pause playback
		{"expect": r"CacheFragment.*track-audio-periodid-881036616", "max": 3, "callback_once":send_command, "callback_arg": "pause"},

		#Wait for a few seconds and resume playback from TSB
		{"expect": r"\[AddFragment\]\[\d+\]\[audio\]", "min": 6, "max": 9, "callback_once":send_command, "callback_arg": "play"},

		#Check we are still fetching from 'std' audio track and switch to alternative
		{"expect": r"CacheFragment.*track-audio.*frag", "min": 9, "callback_once":send_command, "callback_arg": f"set preferredLanguages {sap_track}"},

		#Check that the TSB Store is being flushed
		{"expect": r"TsbStore.*msg=\"Flush triggered\" oldActiveDirNum=\"2\""},

		#Check we are fetching from alternate audio track and finish test
		{"expect": r"CacheFragment.*track-sap-periodid-881036617", "end_of_test":True}
	]
}

"""
Test that when requesting an audio track which is unavailable, playback continues with the current track
"""
TESTDATA2 = {
	"title": "Switching to Unavailable Audio Track",
	"max_test_time_seconds": 15,
	"aamp_cfg": "info=true\nlocalTSBEnabled=true\nprogress=true\ntsbLocation=/tmp/data\ntsbLog=0\n",
	"archive_url": archive_sld_url,
	"url": SLD_URL,
	"cmdlist": ["contentType LINEAR_TV"],
	'simlinear_type': 'DASH',
	"expect_list":
	[
		{"expect": r"aamp_tune", "max":1},

		#Make sure we stay in the right 'mode'
		{"expect": r"Local AAMP TSB 0", "not_expected": True },
		{"expect": r"Local AAMP TSB 1" },
		#Check we are fetching from 'std' audio track and switch to alternative
		{"expect": r"CacheFragment.*track-audio-periodid-881036616", "callback_once":send_command, "callback_arg": f"set preferredLanguages {unavailable_track}"},

		#Check that the TSB Store is not flushed
		{"expect": r"Flush TSB Session Manager", "not_expected": True },

		#Check we continue fetching the same audio track and finish test
		{"expect": r"CacheFragment.*track-audio-periodid-881036617", "end_of_test":True}
	]
}

TESTLIST = [TESTDATA0, TESTDATA1, TESTDATA2]

@pytest.fixture(params=TESTLIST)
def test_data(request):
	return request.param

def test_5005(aamp_setup_teardown, test_data):
	global aamp

	aamp = aamp_setup_teardown
	aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
	aamp.run_expect_b(test_data)

