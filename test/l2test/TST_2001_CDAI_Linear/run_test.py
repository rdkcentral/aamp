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
#Also see README.md

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

AAMP_CMD='/bin/bash -c "' + AAMP_CLI_CMD_PREFIX + AAMP_CLI_PATH


SL_CMD_PORT = os.environ["L2_SL_CMD_PORT"] if "L2_SL_CMD_PORT" in os.environ else '10000'
SL_PORT = os.environ["L2_SL_PORT"] if "L2_SL_PORT" in os.environ else '8085'
SL_PATH=os.path.join(TEST_PATH,'simlinear.py')
SL_CMD=[SL_PATH, '--port', SL_CMD_PORT]
SL_URL= "http://localhost:" + SL_PORT + "/"
SL_DATA_PATH = 'testdata'
OUTPUT_PATH = 'output'

MAX_TEST_TIME_SECS = 300

sl_process = None
httpserver_process = None
cmdlogfile = None
aamp_cli = None
aamp_cfg_saved_file = ""
aamp_cfg_file = os.path.join(AAMP_CFG_DIR, "aamp.cfg")
print("AAMP_CFG_DIR=" + AAMP_CFG_DIR)

HTTP_PORT = os.environ["L2_HTTP_PORT"] if "L2_HTTP_PORT" in os.environ else '8080'
HTTPSERVER_DATA_PATH=os.path.join(TEST_PATH, 'httpdata')
HTTPSERVER_CMD=['python3', '-m', 'http.server', HTTP_PORT, '-d', HTTPSERVER_DATA_PATH]
#HTTPSERVER_CMD=['webfsd', '-F', '-p', HTTP_PORT, '-r', HTTPSERVER_DATA_PATH]
#HTTPSERVER_CMD=['twist3', 'web', '--listen=tcp:' + HTTP_PORT, '--path=' + HTTPSERVER_DATA_PATH]
use_local_httpserver = True

if "TEST_2001_LOCAL_ADS" in os.environ and os.environ["TEST_2001_LOCAL_ADS"] == 'true':
    AD_HOST = "http://localhost:" + HTTP_PORT
    print("Local Ads")
else:
    if "TEST_2001_ADS_PATH" in os.environ:
        AD_HOST = os.environ["TEST_2001_ADS_PATH"]
    else:
        AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"

    use_local_httpserver = False
    print("Remote Ads")


AD_URLS = [
    # skip placement
    "file://skip",

    # 40sec - ad1 (telecoms)
    AD_HOST + "/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd",

    # 60sec - ad2 (lifeboat)
    AD_HOST + "/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd",

    # 30sec - ad3 (bet)
    AD_HOST + "/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd",

    # 10sec - ad4 (movie)
    AD_HOST + "/aamptest/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd",

    # 20sec - ad5 (witness)
    AD_HOST + "/aamptest/ads/ad5/hsar1195-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad2/d14dff37-36d1-4850-aa9d-7d948cbf1fc6/1628318436178/AD/HD/manifest.mpd",

    # 25sec - ad6 (one)
    AD_HOST + "/aamptest/ads/ad6/hsar1103-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD/manifest.mpd",

    # 5sec - ad7 (golf)
    AD_HOST + "/aamptest/ads/ad7/hsar1052-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad3/02e31a39-65cb-41b3-a907-4da24d78eec7/1628264506859/AD/HD/manifest.mpd",
    ]


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
                sl_process = subprocess.Popen(SL_CMD, stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL, cwd=SL_DATA_PATH)

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

def start_httpserver():
    if use_local_httpserver:
        global httpserver_process
        if not os.path.exists(HTTPSERVER_DATA_PATH):
            print("ERROR File does not exist {} Check setup.".format(HTTPSERVER_DATA_PATH))
            sys.exit(os.EX_SOFTWARE)
        try:
            if args.sim_log==True:
                httpserver_process = subprocess.Popen(HTTPSERVER_CMD)
            else:
                httpserver_process = subprocess.Popen(HTTPSERVER_CMD,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        except:
            print("Failed to start http server {}, Check for instance already running".format(HTTPSERVER_CMD))
            sys.exit(os.EX_SOFTWARE)   #Return non-zero


def stop_httpserver():
    global httpserver_process

    if (httpserver_process != None):
        print("Terminating httpserver")
        httpserver_process.terminate()
        httpserver_process = None

def stop_and_exit(code):
    stop_simlinear()
    stop_httpserver()

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
            f.write("info=true\ntrace=true\nlogMetadata=true\nclient-dai=true\n")
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
        if expect_did_happen[j] == False and not "not_expected" in ee:
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
    start_simlinear('DASH')

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

    if (0 == run_num):
        cmdlogfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).parent, Path(testdata["logfile"]).stem) + "_cmd.txt"
    else:
        cmdlogfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).parent, Path(testdata["logfile"]).stem) + "-run" + str(run_num) + "_cmd.txt"

    cmdlogfile = open(cmdlogfile_name,"w")

    for e in testdata["cmdlist"]:
        aamp_sendline(e["cmd"])

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

                if log_start_timestamp==0:
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
    try:
        aamp_sendline("stop")
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


TESTDATA0= {
"title": "Linear CDAI TESTDATA0 alternating",
"logfile": "testdata0.txt",
"url":"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
"cmdlist": [
    {"cmd":"advert add " + AD_URLS[3] + " 30"},
    {"cmd":"advert add " + AD_URLS[0] + " 20"},
    {"cmd":"advert add " + AD_URLS[5] + " 20"},
    {"cmd":"advert add " + AD_URLS[0] + " 10"},
    {"cmd":"advert add " + AD_URLS[2] + " 30"},
    {"cmd":"advert add " + AD_URLS[0]},
    {"cmd":"advert list"},
    ],
"expect_list": [
    # ( string, min time seconds, max time seconds)

    # Period 881036617 - 30sec
    {"expect":"Found CDAI events for period 881036617","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036617\] Duration\[30000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036617 added\[Id=ad0,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad0\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036617\] AdIdx\[0\] Found at Period\[881036617\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036617\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad0\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036617\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036617\] to Queue","min":0, "max":300},

    # Period 881036618 - 20sec
    {"expect":"Found CDAI events for period 881036618","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036618\] Duration\[20000\]","min":0, "max":300},

    # Period 881036619 - 20sec
    {"expect":"Found CDAI events for period 881036619","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036619\] Duration\[20000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036619 added\[Id=ad2,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036619\] AdIdx\[0\] Found at Period\[881036619\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036619\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad2\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036619\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036619\] to Queue","min":0, "max":300},

    # Period 881036620 - 10sec
    {"expect":"Found CDAI events for period 881036620","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036620\] Duration\[10000\]","min":0, "max":300},

    # Period 881036621 - 30sec
    {"expect":"Found CDAI events for period 881036621","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036621\] Duration\[30000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036621 added\[Id=ad4,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad4\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036621\] AdIdx\[0\] Found at Period\[881036621\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036621\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad4\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036621\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036621\] to Queue","min":0, "max":300},


    {"expect":"\[CDAI\] Removing the period\[881036617\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036618\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036619\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036620\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036621\] from mAdBreaks","min":0, "max":300, "end_of_test":True},

    ]
}


TESTDATA1= {
"title": "Linear CDAI TESTDATA1 back2back",
"logfile": "testdata1.txt",
"url":"v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd",
"cmdlist": [
    {"cmd":"advert add " + AD_URLS[3] + " 30"},
    {"cmd":"advert add " + AD_URLS[5] + " 20"},
    {"cmd":"advert add " + AD_URLS[5] + " 20"},
    {"cmd":"advert add " + AD_URLS[4] + " 10"},
    {"cmd":"advert add " + AD_URLS[3] + " 30"},
    {"cmd":"advert add " + AD_URLS[0]},
    {"cmd":"advert list"},
    ],
"expect_list": [
    # ( string, min time seconds, max time seconds)

    # Period 881036617 - 30sec
    {"expect":"Found CDAI events for period 881036617","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036617\] Duration\[30000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036617 added\[Id=ad0,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad0\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036617\] AdIdx\[0\] Found at Period\[881036617\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036617\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad0\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036617\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036617\] to Queue","min":0, "max":300},

    # Period 881036618 - 20sec
    {"expect":"Found CDAI events for period 881036618","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036618\] Duration\[20000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036618 added\[Id=ad1,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad1\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036618\] AdIdx\[0\] Found at Period\[881036618\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036618\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad1\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036618\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036618\] to Queue","min":0, "max":300},


    # Period 881036619 - 20sec
    {"expect":"Found CDAI events for period 881036619","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036619\] Duration\[20000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036619 added\[Id=ad2,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad2\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036619\] AdIdx\[0\] Found at Period\[881036619\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036619\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad2\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036619\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036619\] to Queue","min":0, "max":300},

    # Period 881036620 - 10sec
    {"expect":"Found CDAI events for period 881036620","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036620\] Duration\[10000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036620 added\[Id=ad3,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad3\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036620\] AdIdx\[0\] Found at Period\[881036620\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036620\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad3\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036620\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036620\] to Queue","min":0, "max":300},

    # Period 881036621 - 30sec
    {"expect":"Found CDAI events for period 881036621","min":0, "max":300},
    {"expect":"\[CDAI\] Found Adbreak on period\[881036621\] Duration\[30000\]","min":0, "max":300},
    {"expect":"\[FulFillAdObject\]\[\d+\]New Ad successfully for periodId : 881036621 added\[Id=ad4,","min":0, "max":300},
    {"expect":"\[SendAdResolvedEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Sent resolved status=1 for adId\[ad4\]","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: STARTING ADBREAK\[881036621\] AdIdx\[0\] Found at Period\[881036621\]","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_START\] of adBreakId\[881036621\] to Queue","min":0, "max":300},
    {"expect":"\[SendAdPlacementEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_PLACEMENT_START\] of adId\[ad4\] to Queue.","min":0, "max":300},

    {"expect":"\[onAdEvent\]\[\d+\]\[CDAI\]: All Ads in the ADBREAK\[881036621\] FINISHED","min":0, "max":300},
    {"expect":"\[SendAdReservationEvent\]\[\d+\]PrivateInstanceAAMP: \[CDAI\] Pushed \[AAMP_EVENT_AD_RESERVATION_END\] of adBreakId\[881036621\] to Queue","min":0, "max":300},


    {"expect":"\[CDAI\] Removing the period\[881036617\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036618\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036619\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036620\] from mAdBreaks","min":0, "max":300},
    {"expect":"\[CDAI\] Removing the period\[881036621\] from mAdBreaks","min":0, "max":300, "end_of_test":True},

    ]
}

TESTLIST = [TESTDATA0, TESTDATA1]

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
    start_httpserver()

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
