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
import re


TESTDATA1 = {
    "title": f"Test suite for player commands",
    "logfile": "testdata1.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        #add new player
        {"cmd": "new"},
        {"expect": r"\[AampCurlDownloader\]\[\d+\]Create Curl Downloader Instance"},
        {"expect": r"\[StartScheduler\]\[\d+\]Thread created Async Worker"},
        {"expect": r"\[CreateStreamSink\]\[\d+\]AampStreamSinkManager(.*) Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},
        {"expect": r"\[AddEventListener\]\[\d+\]EventType:0, Listener"},
        {"expect": "new playerInstance; id=1"},

        #add new player with name
        {"cmd": "new peacock"},
        {"expect": r"\[AampCurlDownloader\]\[\d+\]Create Curl Downloader Instance"},
        {"expect": r"\[StartScheduler\]\[\d+\]Thread created Async Worker"},
        {"expect": r"\[CreateStreamSink\]\[\d+\]AampStreamSinkManager(.*) Undefined Pipeline mode, creating GstPlayer for PLAYER\[2\]"},
        {"expect": "Set player name peacock"},
        {"expect": r"\[AddEventListener\]\[\d+\]EventType:0, Listener"},
        {"expect": "new playerInstance; id=2"},

        #select player 1
        {"cmd": "select 1"},
        {"expect": "selected player 1"},

        #select player with name
        {"cmd": "select peacock"},
        {"expect": "selected player 2"},

        #release active player
        {"cmd": "release 2"},
        {"expect": "Can not release the active player"},

        #release player
        {"cmd": "release 1"},
        {"expect": r"\[StopScheduler\]\[\d+\]Stopping Async Worker Thread"},
        {"expect": r"\[ExecuteAsyncTask\]\[\d+\]Exited Async Worker Thread"},
        {"expect": r"\[clearSessionData\]\[\d+\] AampDRMSessionManager:: Clearing session data"},
        {"expect": r"DeleteStreamSink for PLAYER\[1\]"},

        #select player 0
        {"cmd": "select 0"},
        {"expect": r"selected player 0 (.*)"},

        #release player with name
        {"cmd": "release peacock"},
        {"expect": r"\[StopScheduler\]\[\d+\]Stopping Async Worker Thread"},
        {"expect": r"\[ExecuteAsyncTask\]\[\d+\]Exited Async Worker Thread"},
        {"expect": r"\[clearSessionData\]\[\d+\] AampDRMSessionManager:: Clearing session data"},
        {"expect": r"DeleteStreamSink for PLAYER\[2\]"},

    ]
}

TESTDATA2 = {
    "title": f"Test suite for DASH aamp playback commands",
    "logfile": "testdata2.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [

        #tune to url
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": r"\[Tune\]\[\d+]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        
        # Sleep 
        {"cmd": "sleep 2000"},
        {"expect": "sleeping for 2.000000 seconds"},

        # Pause at second
        {"cmd": "pause 10"},
        {"expect": r"\[RunPausePositionMonitoring\]\[\d+]Requested pos 10000ms"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=1.000000"},

        # Pause 
        {"cmd": "pause"},
        {"expect": r"aamp_SetRate\ \(0\.000000\)"},
        {"expect": r"aamp_SetRate rate\(1\.000000\)->\(0\.000000\)"},
        {"expect": "AAMPGstPlayerPipeline state set to PAUSED"}, 
        {"expect": "AAMP_EVENT_STATE_CHANGED: PAUSED"},

        # Seek with pause
        {"cmd": "seek 50 1"},
        {"expect": r"\[SeekInternal\]\[\d+]aamp_Seek\(50\.000000\) and seekToLiveOrEnd\(0\) state\(6\), keep paused\(1\)"},
        {"expect": r"AAMP_EVENT_SEEKED: new positionMs 50000.000000"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate \(1\.000000\)overshoot\(0\)"},

        # Seek 
        {"cmd": "seek 60"},
        {"expect": r"\[SeekInternal\]\[\d+]aamp_Seek\(60\.000000\) and seekToLiveOrEnd\(0\) state\(8\), keep paused\(0\)"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: SEEKING \(7\)"},

        # Slow 
        {"cmd": "slow"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=0.500000."},
        {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=0.500000"},

        # fast forward 
        {"cmd": "ff4"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate rate\(1\.000000\)->\(4\.000000\) cur pipeline: playing. Adj position: 60.000000 Play/Pause Position:60000"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate \(1\.000000\)overshoot\(0\)"},

        # rewind 
        {"cmd": "rew 2"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

        # stop playback
        {"cmd": "stop"},

    ]
}

TESTDATA3 = {
    "title": f"Test suite for HLS aamp playback commands",
    "logfile": "testdata3.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": f"info=true\ntrace=true\nprogress=true\n",
    "expect_list": [
        #tune to url
        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": r"\[Tune\]\[\d+]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: HLS URL: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        
        # Sleep 
        {"cmd": "sleep 2000"},
        {"expect": "sleeping for 2.000000 seconds"},

        # Pause at second
        {"cmd": "pause 10"},
        {"expect": r"\[RunPausePositionMonitoring\]\[\d+]Requested pos 10000ms"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=1.000000"},

        # Pause 
        {"cmd": "pause"},
        {"expect": r"aamp_SetRate\ \(0\.000000\)"},
        {"expect": r"aamp_SetRate rate\(1\.000000\)->\(0\.000000\)"},
        {"expect": "AAMPGstPlayerPipeline state set to PAUSED"}, 
        {"expect": "AAMP_EVENT_STATE_CHANGED: PAUSED"},

        # Seek with pause
        {"cmd": "seek 50 1"},
        {"expect": r"\[SeekInternal\]\[\d+]aamp_Seek\(50\.000000\) and seekToLiveOrEnd\(0\) state\(6\), keep paused\(1\)"},
        {"expect": r"AAMP_EVENT_SEEKED: new positionMs 50000.000000"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate \(1\.000000\)overshoot\(0\)"},

        # Seek 
        {"cmd": "seek 60"},
        {"expect": r"\[SeekInternal\]\[\d+]aamp_Seek\(60\.000000\) and seekToLiveOrEnd\(0\) state\(8\), keep paused\(0\)"},
        {"expect": r"AAMP_EVENT_STATE_CHANGED: SEEKING \(7\)"},

        # Slow 
        {"cmd": "slow"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=0.500000."},
        {"expect": r"AAMP_EVENT_SPEED_CHANGED current rate=0.500000"},

        # fast forward 
        {"cmd": "ff4"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate rate\(1\.000000\)->\(4\.000000\) cur pipeline: playing. Adj position: 60.000000 Play/Pause Position:60000"},

        # Play 
        {"cmd": "play"},
        {"expect": r"\[SetRateInternal\]\[\d+]aamp_SetRate \(1\.000000\)overshoot\(0\)"},

        # rewind 
        {"cmd": "rew 2"},
        {"expect": r"\[SetRateInternal\]\[\d+]PLAYER\[0\] rate=-2.000000."},
        {"expect":r"AAMP_EVENT_SPEED_CHANGED current rate=-2.000000"},

        # stop playback
        {"cmd": "stop"},

    ]
}

TESTDATA4 = {
    "title": "Test suite for aamp playback commands",
    "logfile": "testdata4.txt",
    "max_test_time_seconds": 30,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\nwarn=true",
    "expect_list": [

        # setconfig
        {"cmd": 'setconfig {"warn":true}'},
        {"expect": r"\[Process\]\[\d+]Parsed value for property warn - true"},

        # sessionid 
        {"cmd": "sessionid e13298cc-3ea7-437f-bd91-4f836157e158"},
        {"expect": r"\[AAMPCLI\] SessionId - 0 # e13298cc-3ea7-437f-bd91-4f836157e158"},

        # set bandwidth
        {"cmd": "bps 5000000"},
        {"expect": r"\[SetConfigValue\]\[\d+]abr New Owner\[3\]"},
        {"expect": r"\[SetConfigValue\]\[\d+]initialBitrate New Owner\[3\]"},

        {"cmd": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect" : r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/1080p_001.m4s"},
        {"expect" : r"https://cpetestutility\.stb\.r53\.xcal\.tv/VideoTestStream/dash/1080p_002.m4s"},
        {"cmd": "sleep 2000"},
        
        # progress
        {"cmd": "progress"},
        {"expect": "AAMP_EVENT_PROGRESS"},

        # stats
        {"cmd": "stats"},
        {"expect": r"\[GetPlaybackStats\]\[\d+]Playback stats json"},

        # retune
        {"cmd": "retune"},
        {"expect": r"\[ScheduleRetune\]\[\d+]PrivateInstanceAAMP: Schedule Retune errorType 4 error STARTTIME RESET"},
    ]
}

TESTDATA5 = {
    "title": "Test suite for virtual channel map and auto commands",
    "logfile": "testdata5.txt",
    "max_test_time_seconds": 100,
    "aamp_cfg": "info=true\ntrace=true\nprogress=true\nwarn=true",
    "expect_list": [

        # list
        {"cmd": "list"},
        {"expect": r"\[AAMPCLI\]\ aampcli.cfg virtual channel map:"},
        {"expect": "   1: HOSTED_FRAGMP4         2: HOSTED_HLS             3: HOSTED_DASH"},

        # tune to channel 3
        {"cmd": "3"},
        {"expect": "TUNING to 'HOSTED_DASH' https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": r"\[Tune\]\[\d+]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "sleep 2000"},

        # prev - tune to previous channel 2 
        {"cmd": "prev"},
        {"expect": "TUNING to 'HOSTED_HLS' https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"expect": r"\[Tune\]\[\d+]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: HLS URL: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8"},
        {"cmd": "sleep 2000"},
        {"cmd": 'stop'},
        {"expect": r"\[StopInternal\]\[\d+]aamp_stop PlayerState=8"},

        # next - tune to next channel 3
        {"cmd": "next"},
        {"expect": "TUNING to 'HOSTED_DASH' https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"expect": r"\[Tune\]\[\d+]FOREGROUND PLAYER\[0\] aamp_tune: attempt: 1 format: DASH URL: https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
        {"cmd": "sleep 2000"},
        {"cmd": 'stop'},

        # auto command to run all 3 streams for 10 seconds each
        {"cmd": 'auto 1 3 5 10 5'},
        {"expect": r"\[AAMPCLI\]\ channel number: 1"},
        {"expect": r"\[StopInternal\]\[\d+]aamp_stop PlayerState=8"},
        {"expect": r"\[AAMPCLI\]\ channel number: 2"},
        {"expect": r"\[StopInternal\]\[\d+]aamp_stop PlayerState=8"},
        {"expect": r"\[AAMPCLI\]\ channel number: 3"},
        {"expect": r"\[StopInternal\]\[\d+]aamp_stop PlayerState=8"},

        # exit
        {"cmd": 'exit'},
        {"expect": r"\[StopScheduler\]\[\d+]Stopping Async Worker Thread"},
        {"expect": r"\[ExecuteAsyncTask\]\[\d+]Exited Async Worker Thread"},
        {"expect": r"\[clearSessionData\]\[\d+] AampDRMSessionManager:: Clearing session data"},
        {"expect": r"\[DeleteStreamSink\]\[\d+\]AampStreamSinkManager(.*) DeleteStreamSink for PLAYER\[0\]"},

    ]
}


#TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5]
TESTLIST = [TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5]

############################################################
"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

def test_15001(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
