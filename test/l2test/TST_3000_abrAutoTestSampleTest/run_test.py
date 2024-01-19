#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia Ltd.
#
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
#Starts simlinear, a web server for serving ABR test streams
#Starts aamp-cli and initiates playback by giving it a stream URL
#verifys aamp log output against expected list of events
#
#Also see README.txt

import sys
import os
sys.path.append("../../tools/abrTestingProxy/")

import time
import argparse
import subprocess

import pexpect
import requests
import platform

import proxy_ctrl


#Lib paths needed for AAMP
AAMP_HOME = os.environ["AAMP_HOME"] if "AAMP_HOME" in os.environ else os.path.abspath(os.path.join('..','..','..'))
TEST_PATH = os.path.abspath(os.path.join('.'))
AAMP_CFG_DIR = TEST_PATH
AAMP_CLI_CMD_PREFIX = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''
AAMP_ENV = {"AAMP_CFG_DIR":AAMP_CFG_DIR}

if platform.system() == 'Darwin':
    #MAC
    AAMP_CLI_PATH=os.path.join(AAMP_HOME,'build','Debug','aamp-cli')
else:
    #Linux
    AAMP_ENV.update({"LD_PRELOAD": os.path.join(AAMP_HOME, "Linux", "lib", "libdash.so"), "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, "Linux", "lib")})
    AAMP_CLI_PATH=os.path.join(AAMP_HOME,'Linux','bin','aamp-cli')

AAMP_CMD='/bin/bash -c "' + AAMP_CLI_CMD_PREFIX + AAMP_CLI_PATH + '"'

SL_PORT=8085
SL_URL= f"http://simlinear:{str(SL_PORT)}/"
OUTPUT_PATH = 'output'

MAX_TEST_TIME_SECS = 300

sl_process = {}

proxy = proxy_ctrl.ProxyCtrl()

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
    cmd=[ ("http://simlinear:5000/sim/start", {"port":str(SL_PORT), "type":abr_type}) ]
    status_code = simlinear_cmd(cmd)

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
    print("Terminating simlinear")
    cmd=[ ("http://simlinear:5000/sim/stop", {"port":str(SL_PORT)}) ]
    simlinear_cmd(cmd)
    time.sleep(1)
# Stops the control port, don't want to do that in a container as it will exit the simlinear container, stopping futher tests
#    cmd=[ ("http://simlinear:5000/sim/stop", {"port":"5000"}) ]
#    simlinear_cmd(cmd)   # Stops the control port, don;t want to do that in a container as it will stop future tests
#    time.sleep(1)

def stop_and_exit(code):
    stop_simlinear()
    sys.exit(code)

def create_aamp_cfg():
    """
    $HOME/aamp.cfg should contain
    info=true
    trace=true
    Otherwise aamp-cli will not output the logging required for test validation.
    useTCPServerSink=true See RDKAAMP-48
    networkProxy=http://abrtestproxy:8080
    """
    try:
        if "HOME" in os.environ:
            print("Creating",aamp_cfg_file)
            f = open(aamp_cfg_file,"w")
            f.write("info=true\ntrace=true\nnetworkProxy=http://abrtestproxy:8080/\nuseTCPServerSink=true\n")
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
    but which have not occured in the aamp output
    """
    for j in range(len(expect_did_happen)):
        ee = testdata["expect_list"][j]
        if "needs_tcpserversink" in ee and args.aamp_window == True:
            continue
        if expect_did_happen[j] == False and elapsed > ee["max"] and not "not_expected" in ee:
            print("ERROR {} never occured in expected time window".format(ee))
            return False
    return True

def run_test(testdata,run_num):
    """
    See "Format of TESTDATA" detailed below
    """
    global args
    test_pass=True
    log_start_timestamp=0

    if (0 == run_num):
        logfile_name = os.path.join(OUTPUT_PATH, testdata["logfile"])
    else:
        logfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).stem) + "-run" + str(run_num) + Path(testdata["logfile"]).suffix

    print("{} {}".format(testdata["title"],logfile_name))

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
    expect_list.append("\n(\d{10})\.")

    #start aamp-cli
    EXPECT_TIMEOUT=10
    env = os.environ
    env.update(AAMP_ENV)
    print("AAMP_CMD:",AAMP_CMD)
    aamp = pexpect.spawn(AAMP_CMD, env=env, timeout=EXPECT_TIMEOUT)
    expect_re=aamp.compile_pattern_list(expect_list)

    if args.aamp_log:
        aamp.logfile = sys.stdout.buffer
    elif "logfile" in testdata:
        aamp.logfile = open(logfile_name,"wb")

    #Wait for prompt
    time.sleep(2)
    #aamp.expect_exact('cmd: ')

    #Send URL to start playing
    aamp.sendline(SL_URL+testdata["url"])

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
                        print("ERROR {} occured elapsed={} drift={}".format(e,elapsed,int(time.time()-log_timestamp)))
                        test_pass=False
                    else:
                        #Event occured in window and was expected
                        expect_did_happen[i]=True

                        if "add_rule" in e:
                            proxy.ErrorReply(e["add_rule"][0], e["add_rule"][1], e["add_rule"][2], e["add_rule"][3])

                        if "remove_rule" in e:
                            proxy.RemoveRule(e["remove_rule"])

                        if "set_rate" in e:
                            proxy.SetRate(e["set_rate"])

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
    aamp.sendline("stop")
    try:
        aamp.expect_exact("AAMPGstPlayer_Stop")
        print("Sending exit")
        aamp.sendline("exit")
        aamp.sendeof()
    except:
        print("Exception during shutdown of aamp-cli")
        aamp.kill(9)

    aamp.wait()

    if test_pass:
        result = "PASSED"
    else:
        result = "FAILED"

    print("{} {}".format(result,testdata["title"]))

    return test_pass


###############################################################################
"""
Format of TESTDATA:
 fails if "some_string" DOES NOT occur between min and max (seconds)
 {"expect": "some_string", "min": 55, "max": 60}

 fails if "some_string" DOES occur before min and max (seconds)
 {"expect": "some_string", "min": 55, "max": 60, "not_expected":True}

 End the test
 { ... "end_of_test":True}
"""

TESTDATA1= {
"title": "Canned live HLS playback. Adjust available bandwidth",
"logfile": "testdata1.txt",
"url": "testdata/m3u8s/manifest.1.m3u8",
"expect_list": [
    # ( string, min time seconds, max time seconds)
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "http://simlinear:8085/testdata/m3u8s/discontinuity_test_video_1080_4800000.m3u8", "min": 0, "max": 4}, # Ramp up to the highest bitrate within 3 segment duration (~6 seconds)
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_10.ts", "min": 0, "max": 8, "set_rate": 700},      # First few segment requests should be settled faster than real time and the speed ramped up
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/180_250000/hls/segment_15.ts", "min": 0, "max": 36, "set_rate": 1000},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/270_400000/hls/segment_22.ts", "min": 50, "max": 60, "set_rate": 2100},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/360_800000/hls/segment_27.ts", "min": 65, "max": 80, "set_rate": 20000},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_32.ts", "min": 90, "max": 100, "add_rule": ["token1", ".*video/1080_4800000.*_33.ts", "404", "10"]},
    {"expect": "msg:CDN:VIDEO,HTTP-404 url:http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_33.ts", "min": 90, "max": 104, "remove_rule": "token1"},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/720_2400000/hls/segment_33.ts", "min": 90, "max": 104, "add_rule": ["token2", ".*video/.*_35.ts", "404", "2"]},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/540_1200000/hls/segment_35.ts", "min": 90, "max": 112},
    {"expect": "Got next fragment url http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_40.ts", "min": 100, "max": 135, "add_rule": ["token3", ".*video/.*_41.ts", "55", "1"]},
    {"expect": "Anomaly evt:1 msg:CDN:VIDEO,Curl-55 url:http://simlinear:8085/testdata/m3u8s/../video/1080_4800000/hls/segment_41.ts", "min": 100, "max": 143},
    {"expect": "Buffer is running low", "min": 0, "max": 200, "not_expected" : True},
    {"expect": "fragment injector done. track video", "min": 150, "max": 250, "end_of_test": True }
    ]
}

TESTDATA2= {
"title": "Canned live HLS playback. Introduce error responses",
"logfile": "testdata2.txt",
"url": "testdata/m3u8s/manifest.1.m3u8",
"expect_list": [
    {"expect": "Video Profile added to ABR", "min": 0, "max": 1},
    {"expect": "http://simlinear:8085/testdata/m3u8s/discontinuity_test_video_1080_4800000.m3u8", "min": 0, "max": 4}, # Ramp up to the highest bitrate within 3 segment duration (~6 seconds)
    {"expect": "Buffer is running low", "min": 0, "max": 100, "not_expected" : True},
    {"expect": "fragment injector done. track video", "min": 150, "max": 250, "end_of_test": True }
    ]
}

#The full list of tests
TESTLIST = [TESTDATA1]

#Some subset of tests that we may choose to run
#TESTLIST = [TESTDATA8,TESTDATA9]

parser = argparse.ArgumentParser()
parser.add_argument("--aamp_log", help="Output aamp logging",
                    action="store_true")
parser.add_argument("--sim_log", help="Output sim logging",
                    action="store_true")
parser.add_argument("--aamp_window", help="Run AAMP with window, but no A/V gap detection",
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


if args.aamp_window:
    print("aamp_window option selected. There will be no A/V gap detection")

results={"Pass":0 ,"Fail":0}
create_aamp_cfg()
start_simlinear('HLS')

for r in range(args.repeat):
    for test in testlist:
        res = run_test(test,r)
        if res:
          results["Pass"] +=1
        else:
          results["Fail"] +=1

        if res==False and args.ignore_fails==False:
            print("Results", results)
            stop_and_exit(os.EX_SOFTWARE)   #Return non-zero

print("Results", results)
stop_simlinear()
