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

HELP = """
Used to test the 'tool chain' of harvest, transcode, and playback with a list of URLs
contained in this script

for each URL
    1) harvest some duration of content from that URL
    2) transcode that content (replace encrypted with clear)
    3) Playback that content with aamp. 
    Some playback checks are made by the script but a visual check will also be required. 
    See help for options

For transcode then a 'donor' video needs to be provided. ~/big_buck_bunny_720p_surround.mp4
https://archive.org/download/BigBuckBunny_124/Content/big_buck_bunny_720p_surround.mp4
in the following examples

Example usage:

Harvest 40S of each URL and playback. Expect to see 40S of video played for each URL
test_toolchain.py 

Harvest 60s of every URL containing 'skycdp' (I.E all the encrypted production URLS) and transcode
test_toolchain.py --maxtime 60 --only skycdp --video big_buck_bunny.mp4

Harvest 240S of specified URL (Not from internal list) and transcode.
test_toolchain.py --video ~/big_buck_bunny_720p_surround.mp4 --maxtime 240 https://lin012-gb-s8-prd-ll.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd

"""
sl_process = None

# Find path to aamp repository based on the
# assumption that this script exists within aamp
head = os.path.abspath(sys.argv[0])
tail = ""
while tail != "aamp":
    (head, tail) = os.path.split(head)

AAMP_HOME = os.path.join(head, "aamp")

print("AAMP_HOME=", AAMP_HOME)

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
    time.sleep(1)


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
                            "Potential ERROR file not changed during transcode {}".format(
                                path
                            )
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
    start = time.time()
    donate_video = os.path.abspath(args.video)
    if not os.path.exists(donate_video):
        print("cannot find ", donate_video)
        sys.exit(1)

    # Transcode data
    cmd = [TRANSCODE, "-t", donate_video]
    print(cmd)
    logfile = os.path.join(test_dir, "transcode.log")
    try:
        start_time = time.time()
        log = open(logfile, "wb")
        subprocess.run(cmd, stdout=log, stderr=log, cwd=test_dir, check=True)
        elapsed = time.time() - start_time
        log.write(f"Transcode duration {elapsed} secs\n".encode())
        log.close()
        print(f"Transcode duration {elapsed} secs\n")
    except:
        print("FAILED transcode non zero exit see logfile {}".format(log))
        return False
    return True


###########################################################################
def run_aamp(test_dir, url):
    """
    Start aamp and giv it a URL to play, assumes simlinear already running

    Cannot determine if AAMP is actually managing to output video so
    it is a bit rubbish from a testing point of view.
    """
    first_404 = False
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

    if args.low_bandwidth:
        aamp.sendline("set 37 1000000")
        aamp.expect_exact("cmd: ")

    # Send URL to start playing
    aamp.sendline(url)
    start = time.time()

    passed = True
    expect_list = [r"\n(\d{10})", "LogNetworkError.*url='(.*)'\r"]
    # Keep reading from AAMP otherwise it blocks
    while time.time() - start < playback_time_secs:  # and passed
        try:
            i = aamp.expect(expect_list)
            elapsed = time.time() - start
            if i:
                if i == 1 and first_404 is False:
                    first_404 = True
                    url_404 = aamp.match.group(1).decode()

                    print(
                        "First 404 occurs with URL={} elapsed={}".format(
                            url_404, elapsed
                        )
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
        harvest_start = time.time()
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
            elapsed = time.time() - harvest_start
            print("HARVEST ok: duration", elapsed)
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
        # print("URL={}".format(url))
        if "notes" in data:
            print(data["notes"])

        passed = True
        if harvest(test_dir, data) is False:
            passed = False
            continue

        # Transcode the encrypted streams
        if "is_encrypted" in data and not args.no_trans:
            # check_segments_changed(test_dir, is_before=True)
            if transcode(test_dir, url) is False:
                passed = False
                continue
            # if check_segments_changed(test_dir) is False:
            #   passed = False

        # Skip aamp playback if requested
        if args.no_aamp is False:
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
    #
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentbase.mpd",
        "notes": "Plays more than 40s because of the large base segment",
    },
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main_notimeline.mpd",
    },
    {
        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentlist.mpd",
    },
    {
        "URL": "https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd",
        "harvest_opt": ["--bandwidths", "562800", "--bandwidths", "1328400"],
    },
    #Encrypted
    {
        "notes": "Sky Witness",
        "URL": "https://lin012-gb-s8-prd-ll.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKWITHD_HD_SU_SKYUK_4066_0_6112559918033517163.mpd",
        "is_encrypted": True,
    },
    {
        "notes": "Sky Atlantic",
        "URL": "https://lin012-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
        "is_encrypted": True,
    },
    {
        "notes": "National Geo",
        "URL": "https://lin024-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/NGCHDUK_HD_SU_SKYUK_4031_0_8379280913661561163.mpd",
        "is_encrypted": True,
    },
    # The following has content protection
    # Takes too long to transcode
    #   {
    #       "URL": "https://lin013-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SCINCOH_HD_SU_SKYUK_4019_0_6771210893185225163.mpd",
    #       "is_encrypted" : True,
    #   },
    #   {
    #        "URL": "https://lin022-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/MOV24P_SD_SU_SKYUK_4421_0_5488226467390721163.mpd",
    #        "is_encrypted" : True,
    #    },
    {
        "notes": "Pick HD",
        "URL": "https://lin022-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/PICKTVH_HD_SU_SKYUK_1831_0_8234566181954368163.mpd",
        "is_encrypted": True,
    },
    {
        "notes": "ITV1",
        "URL": "https://lin019-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/ITV1HDL_HD_SU_SKYUK_6504_0_5353835158189364163.mpd",
        "is_encrypted": True,
    },
    # UHD channel
    {
        "URL": "https://lin201-gb-s8-prd-ll.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/UK7201_UD_SU_SKYUK_7201_0_5225302050947417163.mpd",
        "is_encrypted": True,
    },
    # $Time$ template
    {
        "URL": "https://814bffb9b389f652.mediapackage.ap-southeast-2.amazonaws.com/out/v1/eae9d7726eb249f68920dd21203bdb9a/index.mpd",
        "harvest_opt": ["--bandwidths", "249984"],
        "notes": "harvest can take ~30mins because of the large segment buffer in the manifest",
    },
    # HLS
    {
        "URL": "https://cph-p2p-msl.akamaized.net/hls/live/2000341/test/master.m3u8",
    },
]

###################################
parser = argparse.ArgumentParser(
    description=HELP, formatter_class=argparse.RawDescriptionHelpFormatter
)
parser.add_argument(
    "--low_bandwidth",
    help="Limit AAMP bandwidth if playback computer is slow",
    action="store_true",
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
parser.add_argument(
    "--no_aamp",
    help="Skip aamp playback, I.E just do harvest and transcode",
    action="store_true",
)

parser.add_argument(
    "--repeat_forever",
    help="Cycle through the URL(s) until failure is detected",
    action="store_true",
)

parser.add_argument("url", nargs="?", help="url to test in place of internal list")

args = parser.parse_args()

playback_time_secs = args.maxtime * 1.5

"""
min_time_to_404
We only expect aamp to report 404 after this at least this much time. 404 occurs 
because it has read and buffered all harvested segments
"""
min_time_to_404 = 1

if args.url:
    test_urls1 = [{"URL": args.url, "is_encrypted": True}]
elif args.only:
    test_urls1 = []
    for x in TEST_URLS:
        if args.only in x["URL"]:
            test_urls1.append(x)
else:
    test_urls1 = TEST_URLS

try:
    if args.repeat_forever:
        while test(test_urls1) is False:
            pass
    else:
        test(test_urls1)
finally:
    # Try and ensure simlinear is not left running
    stop_simlinear()
