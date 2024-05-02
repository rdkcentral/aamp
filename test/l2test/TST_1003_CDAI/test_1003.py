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

from inspect import getsourcefile
import os
import pytest

TESTDATA1= {
"title": "CDAI Multi Pipeline - Multiple Assets",
"max_test_time_seconds": 60,
"aamp_cfg": "info=true\ntrace=true\nuseSinglePipeline=false\n",
"expect_list": [
   {"cmd": "new"},
   {"expect": r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},
   {"cmd": "new"},
   {"expect": r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[2\]"},
   {"cmd": "new"},
   {"expect": r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[3\]"},
   {"cmd": "new"},
   {"expect": r"Undefined Pipeline mode, creating GstPlayer for PLAYER\[4\]"},
   {"cmd": "autoplay"},
   {"cmd": "select 1"},
   {"expect": r"selected player 1"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd": "select 2"},
   {"expect": r"selected player 2"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd"},
   {"cmd": "play"},
   {"expect": r"Undefined Pipeline, forcing to Multi Pipeline PLAYER\[2\]"},
   {"expect": r"NotifyFirstBufferProcessed"},
   {"cmd": "select 3"},
   {"expect": r"selected player 3"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd"},
   {"cmd": "sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd": "select 2"},
   {"expect": r"selected player 2"},
   {"cmd": "detach"},
   {"cmd": "select 3"},
   {"expect": r"selected player 3"},
   {"cmd": "play"},
   {"expect": r"Multi Pipeline mode, do nothing PLAYER\[3\]"},
   {"expect": r"NotifyFirstBufferProcessed"},
   {"cmd": "select 2"},
   {"expect": r"selected player 2"},
   {"cmd": "stop"},
   {"cmd": "select 4"},
   {"expect": r"selected player 4"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"},
   {"cmd": "sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd": "select 3"},
   {"expect": r"selected player 3"},
   {"cmd": "detach"},
   {"cmd": "select 4"},
   {"expect": r"selected player 4"},
   {"cmd": "play"},
   {"expect": r"Multi Pipeline mode, do nothing PLAYER\[4\]"},
   {"expect": r"NotifyFirstBufferProcessed"},
   {"cmd": "select 3"},
   {"expect": r"selected player 3"},
   {"cmd": "stop"},
   {"cmd": "select 1"},
   {"expect": r"selected player 1"},
   {"cmd": "seek 10"},
   {"cmd": "sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd": "select 4"},
   {"expect": r"selected player 4"},
   {"cmd": "detach"},
   {"cmd": "select 1"},
   {"expect": r"selected player 1"},
   {"cmd": "play"},
   {"expect": r"Multi Pipeline mode, do nothing PLAYER\[1\]"},
   {"expect": r"NotifyFirstBufferProcessed"},
   {"cmd": "select 4"},
   {"expect": r"selected player 4"},
   {"cmd": "stop"},
   {"cmd": "select 1"},
   {"expect": r"selected player 1"},
   {"cmd": "sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd": "stop"},
   {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd": "play"},
   {"expect": r"Multi Pipeline mode, do nothing PLAYER\[1\]"},
   {"expect": r"NotifyFirstBufferProcessed"},
   {"cmd": "sleep 3000"},
   {"expect": r"sleep complete"},
   {"cmd": "stop"},
]
}

############################################################


@pytest.mark.ci_test_set
def test_1003(aamp_setup_teardown):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(TESTDATA1)
