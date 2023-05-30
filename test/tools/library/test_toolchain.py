#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia Ltd.
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
#
import sys
import os
import re
import shutil
import subprocess
import time
import platform
import argparse
import pexpect
HELP="""
Used to test the 'tool chain' of harvest, transcode, and playback with a list of URLs
contained in this script

for each URL
    1) harvest some duration of content from that URL
    2) transcode that content (replace encrypted with clear)
    3) Playback that content with aamp. 
    Some playback checks are made by the script but a visual check will also be required. 
    See help for options


For transcode then a 'donor' video needs to be provided. ~/big_buck_bunny_720p_surround.mp4
in the following examples

Example usage:

Harvest 40S of each URL and playback. No transcode so encrytped content will not playback 
through AAMP although AAMP does appear to read all segments.
test_toolchain.py --no_transcode 

Harvest 40S of each URL, transcode and playback. It takes too long!
test_toolchain.py --video ~/big_buck_bunny_720p_surround.mp4 

Harvest 40S of every URL containing 'ITV' (I.E there is 1 ITV URL in the list ) and transcode
test_toolchain.py --only ITV --video big_buck_bunny.mp4

Harvest 240S of specified URL and transcode.
test_toolchain.py --video ~/big_buck_bunny_720p_surround.mp4  --only ITV --maxtime 240

"""
sl_process = None

#Find path to aamp repository based on the
#assumption that this script exists within aamp
head=os.path.abspath(sys.argv[0])
tail=""
while tail != "aamp":
    (head,tail)=os.path.split(head)

AAMP_HOME=os.path.join(head,"aamp")

print("AAMP_HOME=",AAMP_HOME)

AAMP_ENV = {
    "LD_PRELOAD": AAMP_HOME + "/Linux/lib/libdash.so",
    "LD_LIBRARY_PATH": AAMP_HOME + "/Linux/lib",
}


# New tool layout
TOOLS_HOME = AAMP_HOME + "/test/tools"

HARVEST = TOOLS_HOME + "/harvest/harvest.py"
SL_CMD = TOOLS_HOME + "/simlinear/simlinear.py"
TRANSCODE = TOOLS_HOME + "/replace_segments/transcode.py"

DEFAULT_DONATE_VIDEO = "big_buck_bunny_720p_surround.mp4"

PLAYER = "/snap/bin/vlc"

DEFAULT_HARVEST_DURATION_SEC = 40


#########################################################################
def start_simlinear(abr_type, in_dir):
    """
    Start simlinear web server as a separate process
    """
    global sl_process
    # Start simlinear

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


#################################################################

seg_list = {}


def check_segments_changed(test_dir, is_before=False):
    """
    Called before transcode - records modification time of every file that looks like a segment
    Called after transcode - checks that the modificatiob time of every segment file has changed
    """
    global seg_list
    if is_before:
        seg_list = {}

    for root, _, files in os.walk(test_dir):
        for file in files:
            if re.search(r"(\.ts)|(\.mp[34])|(\.m[34])$", file):
                path = os.path.join(root, file)
                mtime = os.path.getmtime(path)
                if path in seg_list:
                    if mtime == seg_list.get(path):
                        print(
                            "Potential ERROR file not changed during transcode {}".format(path)
                        )
                        return True
                else:
                    seg_list.update({path: mtime})
    return True


def delete_http_host(line):
    """
    Remove host name part from url
    """
    result = re.sub(r"^https?://.+?/", "", line)
    return result


##################################
def transcode(test_dir, url):
    """
    Perform transcode step on harvested data
    """
    donate_video = os.path.abspath(args.video)
    if not os.path.exists(donate_video):
        print("cannot find ", donate_video)
        sys.exit(1)

    path = delete_http_host(url)
    # Transcode data
    cmd = [TRANSCODE, "-a", "-t", donate_video, path]
    print(cmd)
    logfile = os.path.join(test_dir, "transcode.log")
    try:
        start_time = time.time()
        log = open(logfile, "wb")
        subprocess.run(cmd, stdout=log, stderr=log, cwd=test_dir, check=True)
        elapsed = time.time() - start_time
        log.write(f"Transcode duration {elapsed} secs\n".encode())
        log.close()
    except:
        print("FAILED transcode non zero exit see logfile {}".format(log))
        return False

    return True


##################################
def player_and_simlinear(test_dir, url):
    """
    Start some player E.G vlc and simlinear
    """
    playback_url = re.sub(r"(.+?//.+?/)", "http://127.0.0.1:8085/", url)
    print("simlinear playback URL ", playback_url)
    if url.endswith(".mpd"):
        abr_type = "DASH"
    else:
        abr_type = "HLS"

    start_simlinear(abr_type, test_dir)
    cmd = [PLAYER, playback_url]
    player_process = subprocess.Popen(
        cmd, cwd=test_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )

    # By default we are harvesting 40S so let player run for 60s before we kill it
    time.sleep(playback_time_secs)
    print("Terminating player")
    player_process.terminate()

    stop_simlinear()


###########################################################################
def run_aamp(test_dir, url):
    """
    Start aamp and giv it a URL to play, assumes simlinear already running
    """

    aamp = None
    if platform.system() == "Darwin":
        # MAC
        aamp_cmd = AAMP_HOME + "/build/Debug/aamp-cli"
    else:
        # Linux
        aamp_cmd = AAMP_HOME + "/Linux/bin/aamp-cli"

    env = os.environ
    env.update(AAMP_ENV)
    # print(aamp_cmd)
    aamp = pexpect.spawn(aamp_cmd, env=env)
    aamp.logfile = aamp.logfile = open(os.path.join(test_dir, "aamp.log"), "wb")
    # Wait for prompt
    time.sleep(2)
    aamp.expect_exact("cmd: ")

    # Send URL to start playing
    aamp.sendline(url)
    start = time.time()

    passed = True
    expect_list = [r"\n(\d{10})", "LogNetworkError"]
    # Keep reading from AAMP otherwise it blocks
    while time.time() - start < playback_time_secs and passed:
        try:
            i = aamp.expect(expect_list)
            elapsed = time.time() - start
            if i:
                print("{} {}".format(expect_list[i], elapsed))
                if i == 1 and elapsed < min_time_to_404:
                    passed = False
                    print(
                        f"FAILED aamp has logged a 404 which should only occur after {min_time_to_404} secs of play"
                    )
        except pexpect.TIMEOUT:
            pass

    aamp.sendline("stop")

    aamp.sendline("exit")

    aamp.close()
    
    return passed


##################################
def aamp_and_simlinear(test_dir, url):
    """
    Start aamp and simlinear
    """
    playback_url = re.sub(r"(.+?//.+?/)", "http://127.0.0.1:8085/", url)
    print("playback URL ", playback_url)
    if url.endswith(".mpd"):
        abr_type = "DASH"
    else:
        abr_type = "HLS"

    start_simlinear(abr_type, test_dir)

    result_passed = run_aamp(test_dir, playback_url)

    stop_simlinear()

    return result_passed


##################################
def harvest(test_dir, data):
    """
    Run harvest on the URL provided
    """
    test_passed = True
    url = data["URL"]
    bak = test_dir + ".bak"
    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)

    cache = False
    # Cache harvested data instead of fetching every time
    if os.path.exists(bak) and cache:
        print("Copying from ", bak)
        os.system("cp -R " + bak + " " + test_dir)
    else:
        # Harvest from url
        os.makedirs(test_dir, exist_ok=True)
        logfile = os.path.join(test_dir, "harvest.log")
        log = open(logfile, "wb")
        start_time = time.time()
        cmd = (
            [HARVEST]
            + data.get("harvest_opt", [])
            + ["--maxtime", str(args.maxtime), "-r", test_dir, url]
        )
        print(cmd)
        harvest_result = subprocess.run(
            cmd,
            stdout=log,
            stderr=log,
        )
        if harvest_result.returncode != 0:
            print(harvest_result.stderr)
            print("FAILED harvest returned non zero exit code")
            test_passed = False
        else:
            print("HARVEST ok")
            os.system("cp -R " + test_dir + " " + bak)
        elapsed = time.time() - start_time
        log.write(f"\nHarvest duration {elapsed} secs\n".encode())
        log.close()
    return test_passed


#############################################
def test(test_urls):
    """
    Work through a list of URL's and perform harvest, transcode(optional)  and playback
    """

    for idx, data in enumerate(test_urls):
        url = data["URL"]
        test_dir = "harvest_test{}".format(idx)
        print("URL={}".format(url))

        passed = True
        if harvest(test_dir, data) is False:
            passed = False

        # Optionally skip transcode
        if not args.no_trans:
            check_segments_changed(test_dir, is_before=True)
            if transcode(test_dir, url) is False:
                passed = False
            if check_segments_changed(test_dir) is False:
                passed = False

        if args.vlc:
            player_and_simlinear(test_dir, url)
        else:
            if aamp_and_simlinear(test_dir, url) is False:
                passed = False

        if passed:
            print("PASSED {} \n\n".format(test_dir))
        else:
            print("FAILED {} \n\n".format(test_dir))


#####################################################################
TEST_URLS = [
    # DASH
    # Test streams
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentbase.mpd"
    },
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main_notimeline.mpd"
    },
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentlist.mpd"
    },
    {
        "URL": "https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd",
        "harvest_opt": ["--bandwidths", "562800", "--bandwidths", "1328400"],
    },
    # The following has content protection
    {
        "URL": "https://lin013-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SCINCOH_HD_SU_SKYUK_4019_0_6771210893185225163.mpd"
    },
    {
        "URL": "https://lin022-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/MOV24P_SD_SU_SKYUK_4421_0_5488226467390721163.mpd"
    },
    {
        "URL": "https://lin022-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/PICKTVH_HD_SU_SKYUK_1831_0_8234566181954368163.mpd"
    },
    {
        "URL": "https://lin019-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/ITV1HDL_HD_SU_SKYUK_6504_0_5353835158189364163.mpd"
    },
    #UHD channel
    {
        "URL": "https://lin201-gb-s8-prd-ll.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/UK7201_UD_SU_SKYUK_7201_0_5225302050947417163.mpd"
    },
    # HLS
    {
        "URL": "https://cph-p2p-msl.akamaized.net/hls/live/2000341/test/master.m3u8"
    },
    # AAMP don't play{ 'URL':'https://stream.mux.com/v69RSHhFelSm4701snP22dYz2jICy4E4FUyk02rW4gxRM.m3u8'},
    {  # encrypted
        "URL": "https://storage.googleapis.com/shaka-demo-assets/angel-one-widevine-hls/hls.m3u8"
    },
]

###################################
parser = argparse.ArgumentParser(description=HELP,formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument(
    "--vlc", help="Playback using vlc, Default is aamp", action="store_true"
)
parser.add_argument("--no_trans", help="Skip transcoding step", action="store_true")
parser.add_argument(
    "--video",
    help=f"Give video to use for transcode. default={DEFAULT_DONATE_VIDEO}",
    default=DEFAULT_DONATE_VIDEO,
)
parser.add_argument(
    "--only",
    help=f"Select reduced list of urls from test based on string match E.G --only ITV ",
)
parser.add_argument(
    "--maxtime",
    type=int,
    help=f"Duration to harvest for default={DEFAULT_HARVEST_DURATION_SEC}",
    default=DEFAULT_HARVEST_DURATION_SEC,
)
args = parser.parse_args()


playback_time_secs = args.maxtime * 1.5

"""
min_time_to_404
We only expect aamp to report 404 after this at least this much time. 404 occurs 
because it has read and buffered all harvested segments
"""
min_time_to_404 = args.maxtime * 0.8

if args.only:
    test_urls1 = []
    for x in TEST_URLS:
        if args.only in x["URL"]:
            test_urls1.append(x)
else:
    test_urls1 = TEST_URLS

try:
    test(test_urls1)
finally:
    # Try and ensure simlinear is not left running
    stop_simlinear()
