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

from inspect import getsourcefile
import os
import pytest
import re
import subprocess
import shutil
import sys
import time
import pexpect
import framework
import platform
import pytest

HELP = """
1. Used to test the 'tool chain' of harvest, transcode, and playback with a list of URLs
contained in this script

for each URL
    1) harvest some duration of content from that URL
    2) transcode that content if needed (replace encrypted with clear)
    3) Playback that content with aamp. 
    Some playback checks are made by the script but a visual check will also be required. 
    The script will check that the position has moved at least 80% of the duration of the harvest.
"""
sl_process = None

# Find path to aamp repository based on the
# assumption that this script exists within aamp
AAMP_HOME = framework.get_aamp_home()

print("AAMP_HOME=", AAMP_HOME)

AAMP_ENV = {
    "LD_PRELOAD": AAMP_HOME + "/Linux/lib/libdash.so",
    "LD_LIBRARY_PATH": AAMP_HOME + "/Linux/lib",
}

# New tool layout
TOOLS_HOME = AAMP_HOME + "/test/tools"

HARVEST = TOOLS_HOME + "/harvest/harvest.py"
SL_CMD = TOOLS_HOME + "/simlinear/simlinear.py"
TRANSCODE1 = TOOLS_HOME + "/replace_segments/transcode.py"
TRANSCODE2 = TOOLS_HOME + "/transcode_dash/transcode.py"
DEFAULT_DONATE_VIDEO = "big_buck_bunny_720p_surround.mp4"

DEFAULT_HARVEST_DURATION_SEC = 40


#########################################################################
def start_simlinear(abr_type, in_dir):
    """
    Start simlinear web server as a separate process
    """
    global sl_process

    # Kill any existing simlinear process that might have got left running
    # from a previous abnormal exit

    p = subprocess.Popen(['pgrep', '-af', 'simlinear.py'], stdout=subprocess.PIPE)
    out, err = p.communicate()
    if out != b'':
        print("Simlinear already running: -\n" + out.decode("utf-8") + "Attempting to stop it")
        subprocess.run('pgrep -f simlinear.py | xargs -r kill -9', shell=True, executable='/bin/bash')
        time.sleep(1)  # Takes time for cleanup and free of port??

    if abr_type == "HLS":
        opt = "--hls"
    else:
        opt = "--dash"

    cmd = [SL_CMD, opt, "8085"]
    logfile = os.path.join(in_dir, "simlinear.log")
    log = open(logfile, "wb")
    try:
        print(cmd)
        sl_process = subprocess.Popen(cmd, cwd=in_dir, stdout=log, stderr=log)

    except:
        print("Failed to start {}, Check for instance already running".format(SL_CMD))
        sys.exit(os.EX_SOFTWARE)  # Return non-zero


#####################################################################
def stop_simlinear():
    """
    Stop simlinear via process kill
    """
    global sl_process

    if sl_process:
        sl_process.kill()
    sl_process = None
    time.sleep(1)


##################################
def transcode(test_dir, data):
    """
    Perform transcode step on harvested data
    """
    url = data["URL"]
    start = time.time()

    # Transcode data
    transcode_type = data.get("transcode_type", 2)

    if transcode_type == 1:
        donate_video = os.path.abspath(args.video)
        if not os.path.exists(donate_video):
            print("cannot find ", donate_video)
            sys.exit(1)
        cmd = [TRANSCODE1, "-t", donate_video]
    else:
        cmd = [TRANSCODE2, "-v","-a"]

    print(cmd)
    logfile = os.path.join(test_dir, "transcode.log")
    try:
        start_time = time.time()
        log = open(logfile, "wb")
        subprocess.run(cmd, stdout=log, stderr=log, cwd=test_dir, check=True)
        elapsed = time.time() - start_time
        log.write(f"Transcode duration {elapsed} secs\n".encode())
        log.close()
        print(f"Transcode duration {elapsed} secs")
    except Exception:
        with open(logfile, "r") as error_content:
            error = error_content.read()
            print("Transcode error: ", error)
        assert False, "FAILED transcode non zero exit see logfile {}".format(logfile)


###########################################################################
first_position = 0
last_position = 0

def run_aamp(test_dir, url):
    """
    Start aamp and giv it a URL to play, assumes simlinear already running

    Cannot determine if AAMP is actually managing to output video so
    it is a bit rubbish from a testing point of view.
    """

    global first_position, last_position
    first_position = 0
    last_position = 0

    min_expected_playback = DEFAULT_HARVEST_DURATION_SEC*0.8
    fail_msg = None

    aamp = None
    AAMP_ENV = {}
    aamp_cli_cmd_prefix = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''

    # Set to read aamp.cfg from test_dir which will 
    # not contain aamp.cfg to ensure we do not pickup any aamp.cfg
    AAMP_ENV.update({"AAMP_CFG_DIR": test_dir})
    if platform.system() == "Darwin":
        # MAC
        aamp_cmd = AAMP_HOME + "/build/Debug/aamp-cli"
    else:
        # Linux
        AAMP_ENV.update({"LD_PRELOAD": os.path.join(AAMP_HOME, ".libs", "lib", "libdash.so"),
                                  "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, ".libs", "lib")})
        aamp_cli_path = os.path.join(AAMP_HOME, "build", "aamp-cli")
        assert os.path.exists(aamp_cli_path), "ERROR {} does not exist".format(aamp_cli_path)
        AAMP_CMD = '/bin/bash -c "' + aamp_cli_cmd_prefix + aamp_cli_path + '"'
        aamp_cmd = AAMP_CMD

    env = os.environ
    env.update(AAMP_ENV)
    # print(aamp_cmd)
    aamp = pexpect.spawn(aamp_cmd, env=env)
    aamp.logfile = open(os.path.join(test_dir, "aamp.log"), "wb")
    # Wait for prompt
    time.sleep(2)
    aamp.expect_exact("cmd: ")

    if True:
        aamp.sendline("set 37 2000000")
        aamp.expect_exact("cmd: ")

    # Need trace enabled to get "Returning Position as" in log
    aamp.sendline('setconfig {"info":true, "trace":true}')
    aamp.expect_exact("cmd: ")

    # Send URL to start playing
    aamp.sendline(url)
    start = time.time()

    def log_fail(match,elapsed):
        fail_msg = f"Failed on {match.group(0)}"
        return False

    def log_404(match,elapsed):
        #OK to get 404 at the end of the playback because we have finished segments
        if elapsed < min_expected_playback:
            fail_msg = f"Failed on {match.group(0)} elapsed {elapsed}"
            print(fail_msg)
        return False

    def log_position(match,elapsed):
        global first_position, last_position
        last_position = int(match.group(1))
        if first_position == 0:
            first_position = last_position
        return True

    TBL = [
        { "expect": r"\n(\d{10})" },
        { "expect": r",404,", "action": log_404 },
        { "expect": r"AAMP_EVENT_TUNE_FAILED",  "action": log_fail },
        { "expect": r"Returning Position as (\d+)",     "action": log_position },
    ]

    expect_list=[]
    for ent in TBL:
        expect_list.append(ent["expect"])

    keep_running = True
    playback_time_secs = DEFAULT_HARVEST_DURATION_SEC * 1.5
    # Keep reading from AAMP otherwise it blocks
    while time.time() - start < playback_time_secs and keep_running:  # and passed
        try:
            i = aamp.expect(expect_list)
            elapsed = time.time() - start
            if "action" in TBL[i]:
                keep_running = TBL[i]["action"](aamp.match,elapsed)

        except pexpect.TIMEOUT:
            pass
        except Exception as e:
            fail_msg = "Exception {}".format(e)

    # Stop aamp and simlinear before assert so they do not get left running
    aamp.sendline("stop")
    aamp.sendline("exit")
    aamp.close()
    stop_simlinear()

    assert fail_msg is None, fail_msg

    """
    Fail if the aamp reported position has not moved as expected
    """
    p1_secs = first_position/1000
    p2_secs = last_position/1000
    pos_change_sec = p2_secs - p1_secs
    
    if pos_change_sec < min_expected_playback:
        assert False, f"Position has not moved enough p1_secs={p1_secs} p2_secs={p2_secs} min_expected_playback={min_expected_playback}"
    else:
        print(f"Aamp played through pos_change_sec {pos_change_sec}")
    return


##################################
def aamp_and_simlinear(test_dir, data):
    """
    Start aamp and simlinear
    """

    url = data["URL"]
    playback_url = re.sub(r"(.+?//.+?/)", "http://127.0.0.1:8085/", url)
    print("playback URL ", playback_url)
    if url.endswith(".mpd"):
        abr_type = "DASH"
    else:
        abr_type = "HLS"

    start_simlinear(abr_type, test_dir)

    run_aamp(test_dir, playback_url)

##################################
def harvest(test_dir, data):
    """
    Run harvest on the URL provided
    """

    url = data["URL"]

    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)

    # Harvest from url
    os.makedirs(test_dir, exist_ok=True)
    logfile = os.path.join(test_dir, "harvest.log")
    log = open(logfile, "wb")

    cmd = (
            [HARVEST]
            + data.get("harvest_opt", [])
            + ["--maxtime", str(DEFAULT_HARVEST_DURATION_SEC), "-v", "-r", test_dir, url]
    )
    print(cmd)
    harvest_start = time.time()
    harvest_result = subprocess.run(cmd, stdout=log, stderr=log)

    assert harvest_result.returncode ==0 , harvest_result.stderr
 
    elapsed = time.time() - harvest_start
    print("HARVEST ok: duration", elapsed)

    log.write(f"\nHarvest duration {elapsed} secs\n".encode())
    log.close()


############################################################
# Harvesting minimal bitrates make transcode faster
TESTLIST = [

    {
        "notes": "Atlantic",
        "URL": "https://lin012-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
        "is_encrypted": True,
        "harvest_opt": ["-b", "562800", "-b", "117600"],
    },
    {
        "notes": "National Geo",
        "URL": "https://lin024-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/NGCHDUK_HD_SU_SKYUK_4031_0_8379280913661561163.mpd",
        "is_encrypted": True,
        "harvest_opt": ["--no_segments"],
    },
    {
        "notes": "Pick HD",
        "URL": "https://lin022-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/PICKTVH_HD_SU_SKYUK_1831_0_8234566181954368163.mpd",
        "is_encrypted": True,
        "harvest_opt": ["--no_segments","-b", "562800", "-b", "215200", "-b", "605240", "-b", "20000"],
    },
    {
        "notes": "ITV1",
        "URL": "https://lin019-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/ITV1HDL_HD_SU_SKYUK_6504_0_5353835158189364163.mpd",
        "is_encrypted": True,
        "harvest_opt": ["--no_segments","-b", "562800", "-b", "215200", "-b", "605240", "-b", "20000"],
    },
    {
        "notes": "LLD",
        "URL": "https://lin002-gb-s8-stg-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/latency/low/t/UK3054_HD_SU_SKYUK_3054_0_8371500471198371163.mpd",
        "is_encrypted": True,
        "harvest_opt": ["--no_segments", "-b", "20000", "-b", "104192", "-b", "500000"],
    },
# HLS follows
    {
        "URL": "https://d2esudvxtgwpgo.cloudfront.net/v1/master/3fec3e5cac39a52b2132f9c66c83dae043dc17d4/prod-xfinitystream/master.m3u8?ads.xumo_channelId=88889153&ads.csid=xfinitystream_us_xumofreegameshow_ssai&ads.caid=xumofreegameshow&ads.appName=XfinityStream&ads._fw_content_title=&ads._fw_content_category=IAB1-7&ads._fw_content_genre=television&ads._fw_content_language=en&ads._fw_content_rating=tv-14&ads._fw_coppa=0&ads.xumo_adsystem=mediatailor&ads.xumo_providerName=xumofreegameshow&ads.xumo_providerId=5&ads.xumo_contentName=xumofreegameshow&ads.xumo_contentId=5&ads.appVersion=9.2.0&ads._fw_app_bundle=com.comcast.playerplatform.PlayerPlatformTestUI&ads._fw_app_store_url=https://appstore.testui.com&ads._fw_devicetype=Phone&ads._fw_deviceMake=iPhone&ads._fw_device_model=iPhone&ads._fw_is_lat=1&ads.tpcl=MIDROLL&ads._fw_content_length=1",
    },
    {
        "URL": "https://d2it737nair3v7.cloudfront.net/11802/88889523/hls/playlist.m3u8?ads.xumo_channelId=88889523&ads.xumo_streamId=88889523&ads.caid=SkyGenreDocs&ads.csid=xumo_[PLATFORM]_ObsessionMediaOutdoorAmerica_ssai&ads._fw_did=[IFA_TYPE]:[IFA]&ads._fw_is_lat=[IS_LAT]&ads._fw_app_bundle=[APP_BUNDLE]&ads._fw_content_category=[IAB_content_category]&ads._fw_content_genre=[content_genre]&ads._fw_content_language=en&ads._fw_content_rating=[content_rating]&ads._fw_device_model=[device_model]&ads._fw_devicetype=[DEVICETYPE]&ads.appVersion=[APP_VERSION]&ads.xumo_contentId=1096&ads.xumo_contentName=ObsessionMediaOutdoorAmerica&ads.xumo_providerId=1096&ads.xumo_providerName=ObsessionMediaOutdoorAmerica&ads.xumo_channelName=&ads.xumo_platform=[PLATFORM]"
    }
]

############################################################
"""
With this fixture we cause the test to be called
with each entry in TESTLIST
"""
@pytest.fixture(params=TESTLIST)
def test_data(request):
    return request.param

# --slow_tests option needed to run this test
@pytest.mark.slow
def test_0003(test_data):

    this_test_dir, tail = os.path.split(os.path.abspath(getsourcefile(lambda: 0)))
    test_output_dir = os.path.join(this_test_dir,"output",os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0])

    if "notes" in test_data:
        print(test_data["notes"])

    harvest(test_output_dir, test_data)

    # Transcode the encrypted streams
    if "is_encrypted" in test_data:
        transcode(test_output_dir, test_data)

    aamp_and_simlinear(test_output_dir, test_data)
