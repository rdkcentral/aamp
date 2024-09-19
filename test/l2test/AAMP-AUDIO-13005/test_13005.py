import os
import pytest
import time
import json
from inspect import getsourcefile
# Required test streams

## @brief Test case for seamless audio switching during playback.
#  @note This test verifies switching between audio tracks.
TESTDATA1 = {
   "title": "Audio Switch Test",
   "logfile": "AudioSwitch.log",
   "max_test_time_seconds":10,
   "aamp_cfg": "suppressDecode=true",
   "expect_list":
    [
          {"cmd": r"https://ccr.linear-clo-t6-dcf-stg2.xcr.comcast.net/nfl_multiview_dev6.mpd"},
          {"expect": r"aamp_tune"},
          {"expect": r"Successfully parsed Manifest"},
          {"expect": "GetBestAudioTrackByLanguage"},
          {"expect": r"Queueing content protection from StreamSelection"},
          {"expect":"Selected Audio Track: Index:1-0"},
          {"cmd": "set 42 {\"label\":\"screen2_audio_ddplus\",\"language\":\"en\",\"codec\":\"ec-3\",\"rendition\":\"main\",\"bandwidth\":224000,\"Type\":\"audio_screen2_audio_ddplus\",\"availability\":true}"},
          {"expect": r"Matched Command AudioTrack - set 42 {\"label\":\"screen2_audio_ddplus\",\"language\":\"en\",\"codec\":\"ec-3\",\"rendition\":\"main\",\"bandwidth\":224000,\"Type\":\"audio_screen2_audio_ddplus\",\"availability\":true}"},
          {"cmd": "sleep 2000"},
          {"expect":"Selected Audio Track: Index:2-0"},
    ]
}
TESTDATA = [TESTDATA1]
@pytest.fixture(params=TESTDATA)
def test_data(request):
   return request.param
def test_13005(aamp_setup_teardown, test_data):
    aamp = aamp_setup_teardown
    aamp.set_paths(os.path.abspath(getsourcefile(lambda: 0)))
    aamp.run_expect_a(test_data)
