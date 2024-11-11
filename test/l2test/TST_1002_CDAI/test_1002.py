#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events

from inspect import getsourcefile
import os
import pytest
# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

TESTDATA1= {
"title": "CDAI Single Pipeline - Multiple Assets",
"max_test_time_seconds": 60,

"expect_list": [
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json
   # Create main content player - Player 1
   {"cmd":"new"},
   {"expect":r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},

   # Toggle autoplay off
   {"cmd":"autoplay"},


   ############# Pre-roll (one ad) #############

   # Load main content - Player 1
   # (the main content must be loaded before the pre-roll)
   {"cmd":"select 1"},
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json

   {"expect": r"selected player 1"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"expect":r"Pipeline mode set to Single"},
   {"expect":r"Deleting GstPlayer created for PLAYER\[0\]"},
   {"expect":r"Retaining GstPlayer created for PLAYER\[1\]"},

   {"cmd":"detach"},
   {"expect":r"Single Pipeline mode, deactivating active PLAYER\[1\]"},

   # Load and play pre-roll ad - Player 2
   {"cmd":"new"},
   {"expect": r"Single Pipeline mode, not creating GstPlayer for PLAYER\[2\]"},
   {"cmd":"select 2"},
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json

   {"expect": r"selected player 2"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect":r"Single Pipeline mode, no current active player"},
   {"expect":r"Single Pipeline mode, setting active PLAYER\[2\]"},
   {"expect":r"NotifyFirstBufferProcessed"},

   # Play ad for a bit
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   # Transition from pre-roll ad to main content
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[2\]"},
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[1\]"},
   {"expect": r"NotifyFirstBufferProcessed"},

   # Stop and destroy ad player 2
   {"cmd":"select 2"},
   {"expect": r"selected player 2"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, asked to deactivate PLAYER\[2\] when current active PLAYER\[1\]"},
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"release 2"},
   {"expect": r"DeleteStreamSink for PLAYER\[2\]"},

   # Play main content for a bit
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},


   ############# Mid-roll (two ads) #############

   # Pre-load mid-roll ad - Player 3
   {"cmd":"new"},
   {"expect": r"Single Pipeline mode, not creating GstPlayer for PLAYER\[3\]"},
   {"cmd":"select 3"},
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json

   {"expect": r"selected player 3"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd"},

   # Play main content for a bit
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   # Transition from main content to mid-roll ad
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[1\]"},
   {"cmd":"select 3"},
   {"expect": r"selected player 3"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[3\]"},
   {"expect": r"NotifyFirstBufferProcessed"},

   # Play ad for a bit - Player 3
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   # Pre-load mid-roll ad - Player 4
   {"cmd":"new"},
   {"expect": r"Single Pipeline mode, not creating GstPlayer for PLAYER\[4\]"},
   {"cmd":"select 4"},
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json

   {"expect": r"selected player 4"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"},

    # Play ad for a bit - Player 3
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   # Transition from ad to ad
   {"cmd":"select 3"},
   {"expect": r"selected player 3"},
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[3\]"},
   {"cmd":"select 4"},
   {"expect": r"selected player 4"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[4\]"},
   {"expect": r"NotifyFirstBufferProcessed"},

   # Stop and destroy ad player 3
   {"cmd":"select 3"},
   {"expect": r"selected player 3"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, asked to deactivate PLAYER\[3\] when current active PLAYER\[4\]"},
   {"cmd":"select 4"},
   {"expect": r"selected player 4"},
   {"cmd":"release 3"},
   {"expect": r"DeleteStreamSink for PLAYER\[3\]"},

   # Play ad for a bit - Player 4
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   # Transition from mid-roll ad to main content
   {"cmd":"select 4"},
   {"expect": r"selected player 4"},
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[4\]"},
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[1\]"},
   {"expect": r"NotifyFirstBufferProcessed"},

   # Stop and destroy ad player 4
   {"cmd":"select 4"},
   {"expect": r"selected player 4"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, asked to deactivate PLAYER\[4\] when current active PLAYER\[1\]"},
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"release 4"},
   {"expect": r"DeleteStreamSink for PLAYER\[4\]"},

   # Play main content for a bit - Player 1
   {"cmd":"seek 10"},
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},


   ############# Post-roll (one ad) #############

   # Transition from main content to post-roll ad
   # (The client detaches the main content player at EOS and doesn't pre-load the post-roll ad)
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[1\]"},

   # Load and play post-roll ad - Player 5
   {"cmd":"new"},
   {"expect": r"Single Pipeline mode, not creating GstPlayer for PLAYER\[5\]"},
   {"cmd":"select 5"},
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},  # must use " not ' in json

   {"expect": r"selected player 5"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[5\]"},

   # Play ad for a bit - Player 5
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   ############# New main content following post-roll ad #############

   # Transition from post-roll ad to new main content or episode
   # (The client detaches the ad player at EOS)
   {"cmd":"detach"},
   {"expect": r"Single Pipeline mode, deactivating active PLAYER\[5\]"},

   # Stop Player 1
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, no current active PLAYER\[1\]"},

   # Stop and destroy ad player 5
   {"cmd":"select 5"},
   {"expect": r"selected player 5"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, no current active PLAYER\[5\]"},
   {"cmd":"select 1"},
   {"expect": r"selected player 1"},
   {"cmd":"release 5"},
   {"expect": r"DeleteStreamSink for PLAYER\[5\]"},

   # New episode or main content for Player 1
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[1\]"},

   # Play main content for a bit - Player 1
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},

   ############# New main content following main content #############

   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, deactivating and stopping active PLAYER\[1\]"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect": r"Single Pipeline mode, no current active player"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[1\]"},
   {"expect": r"NotifyFirstBufferProcessed"},

   # Play main content for a bit - Player 1
   {"cmd":"sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, deactivating and stopping active PLAYER\[1\]"},
]
}

# This test just checks that if a player is deleted and there are no active
# players then the sink gets attached to a valid (inactive) player. This is to
# avoid a case where a floating pointer can lead to a crash (LLAMA-15915).
TESTDATA2= {
"title": "CDAI Single Pipeline - test player deletion",
"max_test_time_seconds": 10,
"expect_list": [
   {"cmd": 'setconfig {"info":true,"trace":true,"useSinglePipeline":true}'},

   # Play main content
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
   {"expect": r"Single Pipeline mode, already active PLAYER\[0\]"},
   {"cmd":"sleep 3000"},

   # Create a new player and switch to that
   {"cmd":"new"},
   {"expect": r"Single Pipeline mode, not creating GstPlayer for PLAYER\[1\]"},
   {"cmd":"select 1"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
   {"expect": r"Single Pipeline mode, setting active PLAYER\[1\]"},
   {"cmd":"sleep 3000"},

   # Stop and detach the second player
   {"cmd":"stop"},
   {"expect": r"Single Pipeline mode, deactivating and stopping active PLAYER\[1\]"},
   {"cmd":"detach"},
   
   # Delete the second player
   {"cmd":"select 0"},
   {"cmd":"release 1"},
   {"expect": r"DeleteStreamSink for PLAYER\[1\]"},
   {"expect": r"Inactive players present"},

   # See that the sink aamp pointer gets updated to point to a valid player
   {"expect": r"Deleting player associated with sink! Attaching sink to default inactive PLAYER\[0\]"},

   # Stop on player 0 should not crash (although it may look ok if the memory has not been overwritten)
   {"cmd":"stop"},
]
}


############################################################

TESTLIST = [TESTDATA1,TESTDATA2]
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_1002(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)


