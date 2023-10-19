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

#Starts simlinear, a web server for serving ABR test streams
#Starts aamp-cli and initiates playback by giving it a stream URL
#verifies aamp log output against expected list of events
#
#Also see README.txt

import time
import os
import argparse
import sys
import subprocess

import pexpect
import requests
import platform
from pathlib import Path

#paths needed for AAMP

AAMP_HOME = os.environ["AAMP_HOME"] if "AAMP_HOME" in os.environ else os.path.abspath(os.path.join('..','..','..'))
TEST_PATH = os.path.abspath(os.path.join('.'))
AAMP_CFG_DIR = TEST_PATH
AAMP_CLI_CMD_PREFIX = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''
AAMP_ENV = {"AAMP_CFG_DIR":AAMP_CFG_DIR}

if platform.system() == 'Darwin':
    #Mac
    AAMP_CLI_PATH=os.path.join(AAMP_HOME,'build','Debug','aamp-cli')
else:
    #Linux
    AAMP_ENV.update({"LD_PRELOAD": os.path.join(AAMP_HOME, "Linux", "lib", "libdash.so"), "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, "Linux", "lib")})
    AAMP_CLI_PATH=os.path.join(AAMP_HOME,'Linux','bin','aamp-cli')

#Pipe log output from AAMP into tcp_client.py which may add extra log lines for A/V gap detection
AAMP_CMD='/bin/bash -c "' + AAMP_CLI_CMD_PREFIX + AAMP_CLI_PATH + ' | ' + os.path.join(TEST_PATH, 'tcp_client.py') + '"'

SL_CMD_PORT = os.environ["L2_SL_CMD_PORT"] if "L2_SL_CMD_PORT" in os.environ else '10000'
SL_PORT = os.environ["L2_SL_PORT"] if "L2_SL_PORT" in os.environ else '8085'
SL_PATH=os.path.join(TEST_PATH,'simlinear.py')
SL_CMD=[SL_PATH, '--port', SL_CMD_PORT]
SL_URL= "http://localhost:" + SL_PORT + "/"
SL_DATA_PATH = 'testdata'
OUTPUT_PATH = 'output'

MAX_TEST_TIME_SECS = 300

sl_process = None
cmdlogfile = None
aamp_cli = None
aamp_cfg_saved_file = ""
aamp_cfg_file = os.path.join(AAMP_CFG_DIR, "aamp.cfg")
print("AAMP_CFG_DIR=" + AAMP_CFG_DIR)

##############################################################
def start_simlinear(abr_type):
    """
    Start simlinear web server as a separate process
    """
    global sl_process
    global args
    # Start simlinear
    if not os.path.exists(SL_PATH):
        print("ERROR File does not exist {} Check setup.".format(SL_PATH))
        sys.exit(os.EX_SOFTWARE)

    print("Starting simlinear")
    attempt=0
    status_code=0

    while (200 != status_code) and (attempt < 2):
        try:
            if args.sim_log==True:
                sl_process = subprocess.Popen(SL_CMD, cwd=SL_DATA_PATH)
            else:
                sl_process = subprocess.Popen(SL_CMD,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL, cwd=SL_DATA_PATH)

            time.sleep(1)

            #configure simlinear to serve HLS/DASH on a specified port
            # curl - X POST - d '{"port":"8085", "type":"HLS"}' http: // localhost: 5000 / sim / start - H  'Content-Type: application/json' ''',
            sl_server_cmd = 'http://localhost:' + SL_CMD_PORT + '/sim/start'
            cmd=[ (sl_server_cmd, {"port":SL_PORT, "type":abr_type}) ]
            status_code = simlinear_cmd(cmd)
            if (200 != status_code):
                print("FAILED startSimLinear ", status_code)

        except Exception as e:
            print(e)

        if (200 != status_code):
            if (0 == attempt):
                attempt += 1
                p = subprocess.Popen(['pgrep', '-af', 'simlinear.py'], stdout=subprocess.PIPE)
                out, err = p.communicate()
                if (out != b''):
                    print("Simlinear already running: -\n" + out.decode("utf-8") + "Attempt to stop it and try again y/n: ");
                    # not currently interactive
                    #p = subprocess.Popen('pgrep -f simlinear.py | xargs -pr kill -9', shell=True, executable='/bin/bash')
                    p = subprocess.Popen('pgrep -f simlinear.py | xargs -r kill -9', shell=True, executable='/bin/bash')
                    out, err = p.communicate()
                else:
                    print("Failed to start {},\nCheck for instance already running".format(SL_CMD))
                    attempt=2 # nothing found so exit
            else:
                print("Failed to start {},\nCheck for instance already running".format(SL_CMD))
                attempt=2 # nothing found so exit

    if (200 != status_code):
        sys.exit(os.EX_SOFTWARE)   #Return non-zero


def simlinear_cmd(cmd_list):
    """
    Takes list of tuples, feeds then into simlinear
    [ ( url, {json}) , (url, [json]) ...]
    """
    for url,json in cmd_list:
        response = requests.post(url, json=json)
        print("{} {} {}".format(url, json,response.status_code))
        if response.status_code != 200:
            return response.status_code
    return response.status_code

def stop_simlinear():
    global sl_process

    if (sl_process != None):
        print("Terminating simlinear")
        sl_server_cmd = 'http://localhost:' + SL_CMD_PORT + '/sim/stop'
        cmd=[ (sl_server_cmd, {"port":SL_PORT}) ]
        simlinear_cmd(cmd)
        time.sleep(1)
        cmd=[ (sl_server_cmd, {"port":SL_CMD_PORT}) ]
        simlinear_cmd(cmd)
        time.sleep(1)
        sl_process.terminate()
        sl_process = None
        time.sleep(1)


def stop_and_exit(code):
    stop_simlinear()

    sys.exit(code)

################################################################
def create_aamp_cfg():
    """
    $HOME/aamp.cfg should contain
    info=true
    trace=true
    Otherwise aamp-cli will not output the logging required for test validation.
    useTCPServerSink=true See RDKAAMP-48
    """

    try:
        if "HOME" in os.environ:
            print("Creating",aamp_cfg_file)
            f = open(aamp_cfg_file,"w")
            f.write("info=true\ntrace=true\n")
            if args.aamp_video == False:
               f.write("useTCPServerSink=true\n")
            f.close()
        else:
            print("HOME env not set");
            stop_and_exit(os.EX_SOFTWARE)
    except:
        print("ERROR Exception was thrown")
        stop_and_exit(os.EX_SOFTWARE)

def check_for_missed_events(testdata,elapsed,expect_did_happen):
    """
    Check for events that we expect from our test data
    but which have not occurred in the aamp output
    """
    for j in range(len(expect_did_happen)):
        ee = testdata["expect_list"][j]
        if "needs_tcpserversink" in ee and args.aamp_video == True:
            continue
        if expect_did_happen[j] == False and elapsed > ee["max"] and not "not_expected" in ee:
            print("ERROR {} never occurred in expected time window".format(ee))
            return False
    return True

def aamp_sendline(cmd_line):
    global cmdlogfile
    global aamp_cli

    print("CMD: " + cmd_line)
    if (cmdlogfile != None):
        cmdlogfile.write(cmd_line + "\n")

    aamp_cli.sendline(cmd_line)

def run_test(testdata,run_num):
    """
    See "Format of TESTDATA" detailed below
    """
    global args
    global cmdlogfile
    global aamp_cli
    test_pass=True
    log_start_timestamp=0

    if (0 == run_num):
        logfile_name = os.path.join(OUTPUT_PATH, testdata["logfile"])
    else:
        logfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).stem) + "-run" + str(run_num) + Path(testdata["logfile"]).suffix

    print("{} {}".format(testdata["title"],logfile_name))

    stop_simlinear()
    start_simlinear('HLS')

    #Send any test related commands to simlinear
    if "simlinear_cmd" in testdata:
        http_status=simlinear_cmd(testdata["simlinear_cmd"])
        if http_status != 200:
            stop_simLinear()
            return False

    expect_list = []
    expect_did_happen=[]
    #Build list of expected strings from testdata
    for e in testdata["expect_list"]:
        expect_list.append(e["expect"])
        expect_did_happen.append(False)

    #Add a pattern which matches on the timestamp at the beginning of each log line
    expect_list.append("\n(\d{10})")

    #start aamp-cli
    EXPECT_TIMEOUT=10
    env = os.environ
    env.update(AAMP_ENV)
    print(AAMP_CMD)
    aamp = pexpect.spawn(AAMP_CMD, env=env, timeout=EXPECT_TIMEOUT)
    expect_re=aamp.compile_pattern_list(expect_list)
    aamp_cli = aamp

    if args.aamp_log:
        aamp.logfile = sys.stdout.buffer
    elif "logfile" in testdata:
        aamp.logfile = open(logfile_name,"wb")

    #Wait for prompt
    time.sleep(2)
    #aamp.expect_exact('cmd: ')

    if (0 == run_num):
        cmdlogfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).parent, Path(testdata["logfile"]).stem) + "_cmd.txt"
    else:
        cmdlogfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).parent, Path(testdata["logfile"]).stem) + "-run" + str(run_num) + "_cmd.txt"

    cmdlogfile = open(cmdlogfile_name,"w")

    #Send URL to start playing
    aamp_sendline(SL_URL+testdata["url"])

    start_time=time.time()
    end_of_test=False
    while end_of_test==False and test_pass==True:
        try:
            i= aamp.expect_list(expect_re)
        except pexpect.TIMEOUT:
            #Get this exception if no matches for 10secs
            #Use it to do some house keeping
            #We check that all expected log output has occurred.
            if check_for_missed_events(testdata, log_timestamp - log_start_timestamp +EXPECT_TIMEOUT, expect_did_happen) == False:
                test_pass = False

        except:
            print("ERROR Exception was thrown:",str(aamp))
            test_pass=False
            end_of_test = True
            break
        else:
            if i == len(expect_list)-1:
                ##last entry in our list which is the timestamp pattern
                #print(aamp.match.group(0))
                #print(aamp.match.group(1))
                log_timestamp=int(aamp.match.group(1))
            else:
                if i==0 and log_start_timestamp==0:
                    log_start_timestamp = log_timestamp

                elapsed = log_timestamp - log_start_timestamp
                #Get details of the event we just received
                e = testdata["expect_list"][i]
                print("Event {} occurs at elapsed={} drift={}".format(e,elapsed,int(time.time()-log_timestamp)))

                if elapsed >=e["min"] and elapsed <=e["max"]:
                    if "not_expected" in e:
                        #We got event within a time window when we were not expecting it
                        print("ERROR {} occurred elapsed={} drift={}".format(e,elapsed,int(time.time()-log_timestamp)))
                        test_pass=False
                    else:
                        #Event occurred in window and was expected
                        expect_did_happen[i]=True

                        if "end_of_test" in e:
                            if check_for_missed_events(testdata, elapsed, expect_did_happen) == False:
                                test_pass=False
                            end_of_test = True

        finally:
            if (time.time()-start_time) > MAX_TEST_TIME_SECS:
                print("ERROR Max test time exceeded")
                test_pass = False
                end_of_test = True

    #Finish
    aamp_sendline("stop")
    try:
        aamp.expect_exact("AAMPGstPlayer_Stop")
        print("Sending exit")
        aamp_sendline("exit")
        aamp.sendeof()
    except:
        print("Exception during shutdown of aamp-cli")
        aamp.kill(9)

    aamp_cli = None
    aamp.wait()

    if test_pass:
        result = "PASSED"
    else:
        result = "FAILED"

    print("{} {}".format(result,testdata["title"]))

    cmdlogfile.close()

    stop_simlinear()

    return test_pass


###############################################################################
"""
Format of TESTDATA:
 fails if "some_string" DOES NOT occur between main and max (seconds)
 {"expect": "some_string", "min": 55, "max": 60}

 fails if "some_string" DOES occur before min and max (seconds)
 {"expect": "some_string", "min": 55, "max": 60, "not_expected":True}

 End the test
 { ... "end_of_test":True}
"""

TESTDATA1= {
"title": "Canned live HLS playback. No discontinuity",
"logfile": "testdata1.txt",
"url":"m3u8s/manifest.1.m3u8",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect":"Video Profile added to ABR","min":0, "max":1},
   # {"expect": "Buffer is running low", "min": 0, "max": 100, "not_expected" : True},
    {"expect": "fragment injector done. track video", "min": 150, "max": 250,"end_of_test":True }
    ]
}

TESTDATA2= {
"title": "Canned VOD HLS playback. No discontinuity",
"logfile": "testdata2.txt",
"url":"m3u8s_vod/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "Returning Position as 19[0-9]{4}", "min": 190, "max": 300,"end_of_test":True},
    {"expect": "totalInjectedDuration 21[0-1]", "min": 190, "max": 300,"end_of_test":True},
]
}

#https://jira01.engit.synamedia.com/browse/FRDK-145
#hls stream with single audio-only discontinuity
#Audio missing segments 19-21
TESTDATA3= {
"title": "HLS Audio Discontinuity",
"logfile": "testdata3.txt",
"url":"m3u8s_audio_discontinuity_180s/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "#EXT-X-DISCONTINUITY", "min": 40, "max": 100},
    #A section of audio is missing, after this audio buffer state is
    #always red because MonitorBufferHealth has no way of recovering.
    {"expect": "track\[audio\] No Change \[RED\]", "min": 50, "max": 108},
    #Need to check that audio has a gap and no video gap
    {"expect": "Streaming audio gap", "min": 40, "max": 100, "needs_tcpserversink":True},
    #Do not expect video gap but we get one occasionaly. comment to make test pass!
    #{"expect": "Streaming video gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},

    {"expect": "Returning Position as 1[4-5][0-9]{4}", "min": 120, "max": 300,"end_of_test":True},

]
}

#hls stream with single video-only discontinuity
# video segments 19-20 missing
TESTDATA4= {
"title": "HLS Video Discontinuity",
"logfile": "testdata4.txt",
"url":"m3u8s_video_discontinuity_180s/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "#EXT-X-DISCONTINUITY", "min": 40, "max": 80},
    {"expect": "Ignoring discontinuity as audio track does not have discontinuity", "min": 50, "max": 150},
    {"expect": "Streaming video gap", "min": 40, "max": 130, "needs_tcpserversink":True},
    #It is the video that is missing segments, there should not be any problems with audio
    #However we do get audio gaps
    #{"expect": "Streaming audio gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
    {"expect": "track\[audio\] buffering GREEN->YELLOW", "min": 10, "max": 75, "not_expected" : True},
    {"expect": "Returning Position as 148", "min": 120, "max": 300,"end_of_test":True},
    {"expect": "AAMP_EVENT_EOS", "min": 100, "max": 300,"end_of_test":True}

]
}

#hls stream with paired discontinuity in audio/video (i.e. for a content/ad transition)
# Should display segments 6-14, 0-12 = 4*(9+13) =88Secs of play

TESTDATA5= {
"title": "Audio and Video Discontinuity",
"logfile": "testdata5.txt",
"url":"m3u8s_paired_discontinuity_content_transition_108s/manifest.1.m3u8",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect":"Video Profile added to ABR","min":0, "max":1},
    {"expect": "Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": "mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    #{"expect": "fragment injector done. track video", "min": 20, "max": 30, }, #Inject-Stop
    #{"expect": "fragment injector done. track audio", "min": 20, "max": 30, }, #Inject-Stop
    {"expect": "#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
   #Running this test gives a gap in video and audio. The test should fail because of this.
   #But commenting out the following lines to keep the test passing.
   #{"expect": "Streaming audio gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
   #{"expect": "Streaming video gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": "fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect":"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}

#hls live streaming where we see discontinuity in video playlist, but not yet audio playlist.
#Here, we might experience delay before audio playlist is downloaded/refreshed, or receive stale
#audio playlist with new paired audio discontinuity not yet advertised.
#Should display segments 6-14, 0-12
#But playlist audio.*.m3u8.15 will be published 3 seconds late
TESTDATA6= {
"title": "Discontinuity with audio delay",
"logfile": "testdata6.txt",
"url":"m3u8s_paired_discontinuity_audio_3s_108s/manifest.1.m3u8",
#"simlinear_cmd":  [("http://localhost:5000/sim/config2", {"filepath": ".*audio.*\.m3u8\.15", "delay": 3} )] ,
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect":"Video Profile added to ABR","min":0, "max":1},
    {"expect": "Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": "mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    #{"expect": "fragment injector done. track video", "min": 20, "max": 30, }, #Inject-Stop
    #{"expect": "fragment injector done. track audio", "min": 20, "max": 30, }, #Inject-Stop
    {"expect": "#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
   #Running this test gives a gap in video and audio. The test should fail because of this.
   #But commenting out the following lines to keep the test passing.
   #{"expect": "Streaming audio gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
   #{"expect": "Streaming video gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": "fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect":"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}


#hls live streaming where we see discontinuity in audio playlist, but not yet video playlist. Here, we might
#experience delay before video playlist is downloaded/refreshed, or receive stale video playlist with new
#paired video discontinuity not yet advertised.
#Should display segments 6-14, 0-12
#But playlist video.*.m3u8.15 will be published 3 seconds late
TESTDATA7= {
"title": "Discontinuity with video delay",
"logfile": "testdata7.txt",
"url":"m3u8s_paired_discontinuity_video_3s_108s/manifest.1.m3u8",
#"simlinear_cmd":  [("http://localhost:5000/sim/config2", {"filepath": ".*video.*\.m3u8\.15", "delay": 3} )] ,
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect":"Video Profile added to ABR","min":0, "max":1},
    {"expect": "Buffer is running low", "min": 60, "max": 200, "not_expected" : True},
    {"expect": "mTrackState:3!", "min": 20, "max": 35, }, #I.E discontinuity in audio and video
    #{"expect": "fragment injector done. track video", "min": 20, "max": 30, }, #Inject-Stop
    #{"expect": "fragment injector done. track audio", "min": 20, "max": 30, }, #Inject-Stop
    {"expect": "#EXT-X-DISCONTINUITY", "min": 20, "max": 30},
   #Running this test gives a gap in video and audio. The test should fail because of this.
   #But commenting out the following lines to keep the test passing.
   #{"expect": "Streaming audio gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
   #{"expect": "Streaming video gap", "min": 0, "max": 100, "not_expected" : True, "needs_tcpserversink":True},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PAUSED", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayerPipeline PAUSED -> PLAYING", "min": 35, "max": 70},
    {"expect": "AAMPGstPlayer: Pipeline flush seek", "min": 35, "max": 70},
    {"expect": "fragment injector done. track video", "min": 68, "max": 90,"end_of_test":True},
    {"expect":"GST_MESSAGE_EOS","min": 80, "max": 150,"end_of_test":True }
    ]
}


#hls stream with paired discontinuity in audio/video, but with imprecise placement within the respective playlists.
#m3u8s_paired_discontinuity_audio_early_108s
#audio 6-18, 19missing,    20-27
#video 6-18, 19-20missing, 21-27
#
TESTDATA8= {
"title": "HLS Discontinuity audio early",
"logfile": "testdata8.txt",
"url":"m3u8s_paired_discontinuity_audio_early_108s/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "#EXT-X-DISCONTINUITY", "min": 45, "max": 55},
    #Gap of 4 secs for 1 missing video segment when we have corresponding audio
    {"expect": "Streaming video gap 4", "min": 55, "max": 65, "needs_tcpserversink":True},
    #We do not get the EOS marker for this test. Not investigated why. Using another
    #marker to end test
    {"expect":"Returning Position as 104000","min": 80, "max": 150,"end_of_test":True }
]
}

#hls stream with paired discontinuity in audio/video, but with imprecise placement within the respective playlists.
#audio segment 19-20 missing
#video segment 19 missing
TESTDATA9= {
"title": "HLS Discontinuity audio late",
"logfile": "testdata9.txt",
"url":"m3u8s_paired_discontinuity_audio_late_108s/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "#EXT-X-DISCONTINUITY", "min": 45, "max": 75},
    #Gap of 4 secs for 1 missing audio segment when we have video
 #But commenting out the following line to keep the test passing.
    #{"expect": "Streaming audio gap 4", "min": 55, "max": 65, "needs_tcpserversink":True},
    {"expect":"Changing playlist type from\[0\] to ePLAYLISTTYPE_VOD as ENDLIST tag present.","min" :80, "max": 200,"end_of_test":True }
]
}

#The full list of tests
TESTLIST = [TESTDATA1,TESTDATA2,TESTDATA3,TESTDATA4,TESTDATA5,TESTDATA6,TESTDATA7,TESTDATA8,TESTDATA9]

#Some subset of tests that we may choose to run
#TESTLIST = [TESTDATA8,TESTDATA9]

parser = argparse.ArgumentParser()
parser.add_argument("--aamp_log", help="Output aamp logging",
                    action="store_true")
parser.add_argument("--sim_log", help="Output sim logging",
                    action="store_true")
parser.add_argument("-v","--aamp_video", help="Run AAMP with video window, but no A/V gap detection",
                    action="store_true")
parser.add_argument("--ignore_fails", help="Continue with next test even if previous failed. Default option is to exit on failure",
                    action="store_true")
parser.add_argument("--only", type=int, choices=range(1,len(TESTLIST)), help="Limit run to a single test")
parser.add_argument('--repeat', type=int, default=1, help='Repeat the set of tests n times')

args = parser.parse_args()

if args.only:
    testlist=[ TESTLIST[args.only-1]]
else:
    testlist=TESTLIST

if args.aamp_video:
    print("AAMP with video window option selected. There will be no A/V gap detection")

if not os.path.exists(AAMP_CLI_PATH):
    print("ERROR cannot access AAMP_CLI_PATH={}".format(AAMP_CLI_PATH))
    sys.exit(os.EX_SOFTWARE)

#URL's used by simlinear all expect to read from 'testdata' directory
if not os.path.exists(SL_DATA_PATH):
    print("ERROR cannot access directory SL_DATA_PATH={} Check env setup".format(SL_DATA_PATH))
    sys.exit(os.EX_SOFTWARE)

if not os.path.exists(OUTPUT_PATH):
    os.mkdir(OUTPUT_PATH)
else:
    for f in Path(OUTPUT_PATH).glob("testdata*.txt"):
        f.unlink()

if not os.path.exists(OUTPUT_PATH):
    print("ERROR cannot access directory OUTPUT_PATH={}".format(OUTPUT_PATH))
    sys.exit(os.EX_SOFTWARE)

if not os.path.exists(AAMP_CFG_DIR):
    print("ERROR cannot access directory AAMP_CFG_DIR={}".format(AAMP_CFG_DIR))
    sys.exit(os.EX_SOFTWARE)

results={"Pass":0 ,"Fail":0}
results_detail={}

try:
    create_aamp_cfg()

    for r in range(args.repeat):
        for test in testlist:
            name = test['logfile']
            if name not in results_detail:
                results_detail[name] = []

            res = run_test(test,r)
            if res:
                results["Pass"] +=1
                results_detail[name].append('P')
            else:
                results["Fail"] +=1
                results_detail[name].append('F')

            if res==False and args.ignore_fails==False:
                stop_and_exit(os.EX_SOFTWARE)   #Return non-zero

    for name in results_detail:
        print(name, results_detail[name])

    print("Results", results)
    stop_and_exit(0)

except Exception as e:
    print("ERROR Exception was thrown: %s" % (e))
    stop_and_exit(os.EX_SOFTWARE)   #Return non-zero

