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
1. Used to test the 'tool chain' of harvest, transcode, and playback with a list of URLs
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

2. Also used to test Video test streams.
    1. generate-hls-dash.sh is executed from test/VideoTestStream to harvest manifests.
    2. Simlinear server is started to host required URLs.
    3. URLs are run one-by-one in aamp-cli to test the playback.


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
        return "FAILED transcode non zero exit see logfile {}".format(logfile)
    return None

###########################################################################
first_position = 0
last_position = 0
count_of_404 = 0
def run_aamp(test_dir, url):
    """
    Start aamp and giv it a URL to play, assumes simlinear already running

    Cannot determine if AAMP is actually managing to output video so
    it is a bit rubbish from a testing point of view.
    """

    global first_position, last_position,count_of_404
    first_position = 0
    last_position = 0
    count_of_404 = 0

    aamp = None
    AAMP_ENV = {}
    aamp_cli_cmd_prefix = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''

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

    if args.low_bandwidth:
        aamp.sendline("set 37 2000000")
        aamp.expect_exact("cmd: ")

    aamp.sendline('setconfig {"info":true, }')
    aamp.expect_exact("cmd: ")

    # Send URL to start playing
    aamp.sendline(url)
    start = time.time()

    fail_msg = None

    def log_fail(match,elapsed):
        return f"Failed on {match.group(0)}"

    def log_position(match,elapsed):
        global first_position, last_position
        last_position = int(match.group(1))
        if first_position == 0:
            first_position = last_position
        return None

    TBL = [
        { "expect": r"\n(\d{10})" },
        { "expect": r"AAMP_EVENT_TUNE_FAILED",  "action": log_fail },
        { "expect": r"Returning Position as (\d+)",     "action": log_position },
    ]

    expect_list=[]
    for ent in TBL:
        expect_list.append(ent["expect"])

    # Keep reading from AAMP otherwise it blocks
    while time.time() - start < playback_time_secs:  # and passed
        try:
            i = aamp.expect(expect_list)
            elapsed = time.time() - start
            if "action" in TBL[i]:
                msg = TBL[i]["action"](aamp.match,elapsed)
                if msg and fail_msg is None:
                    fail_msg = msg

        except pexpect.TIMEOUT:
            pass

    aamp.sendline("stop")
    aamp.sendline("exit")
    aamp.close()

    """
    Fail if the aamp reported position has not moved as expected
    """
    p1_secs = first_position/1000
    p2_secs = last_position/1000
    pos_change_sec = p2_secs - p1_secs
    expected_move = args.maxtime/2
    if pos_change_sec < expected_move and fail_msg is None:
        fail_msg = f"Position has not moved enough p1_secs={p1_secs} p2_secs={p2_secs} expect_move={expected_move}"
    else:
        print(f"Aamp played through pos_change_sec {pos_change_sec}")
    return fail_msg


##################################
def aamp_and_simlinear(test_dir, data):
    """
    Start aamp and simlinear
    """
    fail_msg = None
    url = data["URL"]
    playback_url = re.sub(r"(.+?//.+?/)", "http://127.0.0.1:8085/", url)
    print("playback URL ", playback_url)
    if url.endswith(".mpd"):
        abr_type = "DASH"
    else:
        abr_type = "HLS"

    start_simlinear(abr_type, test_dir)

    fail_msg = run_aamp(test_dir, playback_url)

    stop_simlinear()

    return fail_msg


##################################
def harvest(test_dir, data):
    """
    Run harvest on the URL provided
    """
    failed_msg = None
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
            + ["--maxtime", str(args.maxtime), "-v", "-r", test_dir, url]
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
            return "FAILED harvest returned non zero exit code"
        else:
            elapsed = time.time() - harvest_start
            print("HARVEST ok: duration", elapsed)
            os.system("cp -R " + test_dir + " " + bak)
        elapsed = time.time() - start_time
        log.write(f"\nHarvest duration {elapsed} secs\n".encode())
        log.close()
    return failed_msg


exitStatus = 0
failedTests = []

def record_if_failure(dir,msg):
    if msg:
        print(f"{dir} {msg}")
        failedTests.append(f"{dir} {msg}")
        exitStatus = 1
        return True
    return False
#############################################
def test(test_urls):
    """
    Work through a list of URL's and perform harvest, transcode(optional)  and playback
    """
    global exitStatus

    for idx, data in enumerate(test_urls):
        url = data["URL"]
        test_dir = "harvest_test{}".format(idx)
        # print("URL={}".format(url))
        print("\n")
        if "notes" in data:
            print(data["notes"])


        if record_if_failure(test_dir,harvest(test_dir, data)):
            continue

        # Transcode the encrypted streams
        if "is_encrypted" in data and not args.no_trans:
            # check_segments_changed(test_dir, is_before=True)
            if record_if_failure(test_dir,transcode(test_dir, data)):
                continue


        # Skip aamp playback if requested
        if args.no_aamp is False:
            record_if_failure(test_dir,aamp_and_simlinear(test_dir, data))


def test_VideoTestStream():

    global exitStatus
    print("Starting VideoTestStreams")
    os.chdir('../../VideoTestStream')

    print("Harvesting...")
    os.system('./generate-hls-dash.sh')

    print("Starting simlinear server...")
    server_process = subprocess.Popen(['./startserver.sh'], shell=True)

    os.chdir('../tools/library')
    for idx, url in enumerate(VIDEO_TEST_STREAM_URLS):
        test_url = url["URL"]
        test_dir = "VideoTestStream_test{}".format(idx)
        os.makedirs(test_dir, exist_ok=True)
        if run_aamp(test_dir, test_url): 
            print("PASSED {} \n\n".format(test_dir))

        else:
            print("FAILED {} \n\n".format(test_dir))
            failedTests.append(test_dir)
            exitStatus = 1

    print("Stopping simlinear server")
    server_process.terminate()
    server_process.kill()


#####################################################################
TEST_URLS = [
    # DASH
    # Test streams
    #

 # Partial fetches do not work over simlinear
 # http://127.0.0.1:8085/VideoTestStream/dash/480.mp4;838-6269
 #   {
 #       "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentbase.mpd",
 #        "notes": "Plays more than 40s because of the large base segment",
 #   },

 # harvest got stuck
 #   {
 #       "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main_notimeline.mpd",
 #   },

 # Harvest gets stuck takes forever
 #    {
 #        "URL": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main-segmentlist.mpd",
 #    },

    {
        "URL": "https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd",
        "harvest_opt": ["--bandwidths", "562800", "--bandwidths", "1328400"],
    },
    {
        "notes": "Atlantic",
        "URL": "https://lin012-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
        "is_encrypted": True,
    },
    {
        "notes": "National Geo",
        "URL": "https://lin024-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/NGCHDUK_HD_SU_SKYUK_4031_0_8379280913661561163.mpd",
        "is_encrypted": True,
        "transcode_type": 1
    },
    {
        "notes": "National Geo",
        "URL": "https://lin024-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/NGCHDUK_HD_SU_SKYUK_4031_0_8379280913661561163.mpd",
        "is_encrypted": True,
        "transcode_type": 2
    },
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
    
    # returns 403
    #{
    #    #Live
    #    "URL": "https://linear.stvacdn.spectrum.com/LIVE/5105/bpk-tv/10370/drm/manifest.mpd?iptvCustomer=true&adId=d45b7b9b-4edb-42e1-afcb-25a47da7a4d6",
    #    "is_encrypted": True,
    #},
   #HLS Test streams
   # aamp cannot play
   # {
   #     "URL": "https://cph-p2p-msl.akamaized.net/hls/live/2000341/test/master.m3u8",
   # },
    {
        "URL": "https://d2esudvxtgwpgo.cloudfront.net/v1/master/3fec3e5cac39a52b2132f9c66c83dae043dc17d4/prod-xfinitystream/master.m3u8?ads.xumo_channelId=88889153&ads.csid=xfinitystream_us_xumofreegameshow_ssai&ads.caid=xumofreegameshow&ads.appName=XfinityStream&ads._fw_content_title=&ads._fw_content_category=IAB1-7&ads._fw_content_genre=television&ads._fw_content_language=en&ads._fw_content_rating=tv-14&ads._fw_coppa=0&ads.xumo_adsystem=mediatailor&ads.xumo_providerName=xumofreegameshow&ads.xumo_providerId=5&ads.xumo_contentName=xumofreegameshow&ads.xumo_contentId=5&ads.appVersion=9.2.0&ads._fw_app_bundle=com.comcast.playerplatform.PlayerPlatformTestUI&ads._fw_app_store_url=https://appstore.testui.com&ads._fw_devicetype=Phone&ads._fw_deviceMake=iPhone&ads._fw_device_model=iPhone&ads._fw_is_lat=1&ads.tpcl=MIDROLL&ads._fw_content_length=1",
    },
    {
        "URL": "https://storage.googleapis.com/shaka-demo-assets/angel-one-hls/hls.m3u8"
    }
]

# VideoTestStream
"""
These are kept separate from rest of the URLs because the testing flow is different.
(No harvest and transcode is needed)
"""
VIDEO_TEST_STREAM_URLS = [
    {
        "URL": "http://127.0.0.1:8080/main.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main.mpd",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mp4.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mp4.m3u8",
    },
    {
        "URL": "http://127.0.0.1:8080/main_mux.m3u3",
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
    help=f"Select reduced list of urls from test based on string match E.G --only ITV or --only m3u8",
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
    "--vts",
    help="test VideoTestStream",
    action="store_true",
)

parser.add_argument(
    "--repeat_forever",
    help="Cycle through the URL(s) until failure is detected. Live streams may cause failure at specific point",
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

if __name__ == "__main__":
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

    # Run VideoTestStream tests
    if args.vts:
        test_VideoTestStream()

    if failedTests:
        print("FAILED TESTS:")
        for f in failedTests:
            print(f)
    else:
        print("ALL TESTS PASSED")

    sys.exit(exitStatus)