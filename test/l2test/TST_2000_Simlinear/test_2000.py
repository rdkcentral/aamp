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

# Starts simlinear, a web server for serving ABR test streams
# Starts aamp-cli and initiates playback by giving it a stream URL
# verifies aamp log output against expected list of events
#
# Also see README.txt

from inspect import getsourcefile
import os
import pytest

###############################################################################

archive_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/simlinear/streams09-Jan2023.tar.gz"

TESTDATA1 = {
"title": "Canned live HLS playback. No discontinuity",
"archive_url": archive_url,
"url": "m3u8s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect":"Video Profile added to ABR","min":0, "max":1},
    {"expect": r"Returning Position as (\d+) ","min":0, "max":300},
    {"expect": r"fragment injector done. track video", "min": 150, "max": 250,"end_of_test":True }
    ]
}

TESTDATA2 = {
"title": "Canned VOD HLS playback. No discontinuity",
"archive_url": archive_url,
"url": "m3u8s_vod/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": r"Returning Position as (\d+) ","min":0, "max":300},
    {"expect": r"Returning Position as 19[0-9]{4}", "min": 190, "max": 300,"end_of_test":True},
    {"expect": r"totalInjectedDuration 21[0-1]", "min": 190, "max": 300,"end_of_test":True},
]
}

#https://jira01.engit.synamedia.com/browse/FRDK-145
#hls stream with single audio-only discontinuity
#Audio missing segments 19-21
TESTDATA3= {
"title": "HLS Audio Discontinuity",
"archive_url": archive_url,
"url": "m3u8s_audio_discontinuity_180s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 40, "max": 100},
    #A section of audio is missing, after this audio buffer state is
    #always red because MonitorBufferHealth has no way of recovering.
    {"expect": r"track\[audio\] No Change \[RED\]", "min": 50, "max": 108},
    #Need to check that audio has a gap and no video gap

    {"expect": r"Returning Position as 1[4-5][0-9]{4}", "min": 120, "max": 300,"end_of_test":True},

]
}

#hls stream with single video-only discontinuity
# video segments 19-20 missing
TESTDATA4= {
"title": "HLS Video Discontinuity",
"archive_url": archive_url,
"url": "m3u8s_video_discontinuity_180s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 40, "max": 80},
    {"expect": r"Ignoring discontinuity as audio track does not have discontinuity", "min": 50, "max": 150},
    {"expect": r"track\[audio\] buffering GREEN->YELLOW", "min": 10, "max": 75, "not_expected" : True},
    {"expect": r"Returning Position as 148", "min": 120, "max": 300,"end_of_test":True},
    {"expect": r"AAMP_EVENT_EOS", "min": 100, "max": 300,"end_of_test":True}
]
}

#hls stream with paired discontinuity in audio/video (i.e. for a content/ad transition)
# Should display segments 6-14, 0-12 = 4*(9+13) =88Secs of play

TESTDATA5= {
"title": "Audio and Video Discontinuity",
"archive_url": archive_url,
"url": "m3u8s_paired_discontinuity_content_transition_108s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": r"Video Profile added to ABR","min":0, "max":1},
    {"expect": r"Returning Position as (\d+) ", "min": 0, "max": 300},
    {"expect": r"Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": r"mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": r"fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect": r"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}

#hls live streaming where we see discontinuity in video playlist, but not yet audio playlist.
#Here, we might experience delay before audio playlist is downloaded/refreshed, or receive stale
#audio playlist with new paired audio discontinuity not yet advertised.
#Should display segments 6-14, 0-12
#But playlist audio.*.m3u8.15 will be published 3 seconds late
TESTDATA6= {
"title": "Discontinuity with audio delay",
"archive_url": archive_url,
"url": "m3u8s_paired_discontinuity_audio_3s_108s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": r"Video Profile added to ABR","min":0, "max":1},
    {"expect": r"Returning Position as (\d+) ", "min": 0, "max": 300},
    {"expect": r"Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": r"mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": r"fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect": r"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}


#hls live streaming where we see discontinuity in audio playlist, but not yet video playlist. Here, we might
#experience delay before video playlist is downloaded/refreshed, or receive stale video playlist with new
#paired video discontinuity not yet advertised.
#Should display segments 6-14, 0-12
#But playlist video.*.m3u8.15 will be published 3 seconds late
TESTDATA7= {
"title": "Discontinuity with video delay",
"archive_url": archive_url,
"url": "m3u8s_paired_discontinuity_video_3s_108s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": r"Video Profile added to ABR","min":0, "max":1},
    {"expect": r"Returning Position as (\d+) ", "min": 0, "max": 300},
    {"expect": r"Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": r"mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": r"AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": r"fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect": r"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}


#hls stream with paired discontinuity in audio/video, but with imprecise placement within the respective playlists.
#m3u8s_paired_discontinuity_audio_early_108s
#audio 6-18, 19missing,    20-27
#video 6-18, 19-20missing, 21-27
#
TESTDATA8= {
"title": "HLS Discontinuity audio early",
"archive_url": archive_url,
"url": "m3u8s_paired_discontinuity_audio_early_108s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 45, "max": 55},
    #Gap of 4 secs for 1 missing video segment when we have corresponding audio
    #marker to end test
    {"expect": r"Returning Position as 104000","min": 80, "max": 150,"end_of_test":True }
]
}

#hls stream with paired discontinuity in audio/video, but with imprecise placement within the respective playlists.
#audio segment 19-20 missing
#video segment 19 missing
TESTDATA9= {
"title": "HLS Discontinuity audio late",
"archive_url": archive_url,
"url":"m3u8s_paired_discontinuity_audio_late_108s/manifest.1.m3u8",
"simlinear_type": "HLS",
"max_test_time_seconds": 300,
"aamp_cfg": "info=true\ntrace=true\n",
"expect_list": [
    {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": r"Returning Position as (\d+) ", "min": 0, "max": 300},
    {"expect": r"#EXT-X-DISCONTINUITY", "min": 45, "max": 75},
    # Gap of 4 secs for 1 missing audio segment when we have video
    # But commenting out the following line to keep the test passing.
    {"expect": r"Changing playlist type from\[0\] to ePLAYLISTTYPE_VOD as ENDLIST tag present.","min" :80, "max": 200,"end_of_test":True }
]
}

#The full list of tests
TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5,TESTDATA6,TESTDATA7,TESTDATA8,TESTDATA9]


############################################################


"""
With this fixture we cause the test to be called 
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param


def test_2000(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    print(test_data['title'])
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_b(test_data)

