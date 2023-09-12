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

# Starts aamp-cli and initiates playback by giving it a stream URL
# verifys aamp log output against expected list of events

import time
import os
import argparse
import sys
import subprocess

import pexpect
import requests

#Lib paths needed for AAMP

if "AAMP_HOME" in os.environ:
    AAMP_HOME = os.environ["AAMP_HOME"]
else:
    AAMP_HOME = os.path.join(os.environ["PWD"], "..", "..", "..")

print("AAMP_HOME:",AAMP_HOME)

if os.path.exists(os.path.join(AAMP_HOME, "Linux", "bin", "aamp-cli")):
    AAMP_ENV = {"LD_PRELOAD": os.path.join(AAMP_HOME, "Linux", "lib", "libdash.so"), "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, "Linux", "lib")}
    AAMP_CMD = os.path.join(AAMP_HOME, "Linux", "bin", "aamp-cli")
    print("Platform: Ubuntu")
elif os.path.exists(os.path.join(AAMP_HOME, "build", "Debug", "aamp-cli")):
    AAMP_ENV = ""
    AAMP_CMD = os.path.join(AAMP_HOME, "build", "Debug", "aamp-cli")
    print("Platform: Mac")
else:
    print("ERROR: aamp-cli not found")
    sys.exit(os.EX_SOFTWARE)

MAX_TEST_TIME_SECS = 60

aamp_cfg_file = os.path.join(os.environ["HOME"], "aamp.cfg")
aamp_cfg_saved_file = ""

################################################################
def save_aamp_cfg():
    try:
        if "HOME" in os.environ:
            if os.path.isfile(aamp_cfg_file):
                # If aamp.cfg.saved already exists, try aamp.cfg.saved_1,
                # aamp.cfg.saved_2... until we find one that does not exist.
                global aamp_cfg_saved_file
                aamp_cfg_saved_file = os.path.join(os.environ["HOME"], "aamp.cfg.saved")
                temp_aamp_cfg_saved_file = aamp_cfg_saved_file
                file_index = 1
                while os.path.isfile(temp_aamp_cfg_saved_file):
                    temp_aamp_cfg_saved_file = aamp_cfg_saved_file + "_" + str(file_index)
                    file_index += 1
                aamp_cfg_saved_file = temp_aamp_cfg_saved_file
                print("Rename", aamp_cfg_file, "to", aamp_cfg_saved_file)
                os.rename(aamp_cfg_file, aamp_cfg_saved_file)
        else:
            print("HOME env not set")
            sys.exit(os.EX_SOFTWARE)
    except Exception as e:
        print("ERROR Exception was thrown in save_aamp_cfg(): %s" % (e))
        sys.exit(os.EX_SOFTWARE)

def restore_aamp_cfg():
    try:
        if aamp_cfg_saved_file:
            print("Restore", aamp_cfg_file, "from", aamp_cfg_saved_file)
            os.rename(aamp_cfg_saved_file, aamp_cfg_file)
        else:
            os.remove(aamp_cfg_file)
    except Exception as e:
        print("ERROR Exception was thrown in restore_aamp_cfg(): %s" % (e))
        sys.exit(os.EX_SOFTWARE)

################################################################
#$HOME/aamp.cfg should contain
#info=true
#trace=true
# Otherwise aamp-cli will not output the logging required for test validation.
#useTCPServerSink=true
# Required to run the test in CI
def create_aamp_cfg():
    try:
        if "HOME" in os.environ:
            print("Creating ", aamp_cfg_file)
            f = open(aamp_cfg_file, "w")
#            f.write("info=true\ntrace=true\nuseTCPServerSink=true\nuseSinglePipeline=true\n")
            f.write("info=true\ntrace=true\nuseTCPServerSink=false\nuseSinglePipeline=true\n")
            f.close()
        else:
            print("HOME env not set")
            sys.exit(os.EX_SOFTWARE)
    except Exception as e:
        print("ERROR Exception was thrown in create_aamp_cfg(): %s" % (e))
        sys.exit(os.EX_SOFTWARE)

#####################################################################
def run_test(testdata):
    global args
    test_pass=True
    print("{} {}".format(testdata["title"],testdata["logfile"]))

    #start aamp-cli

    env = os.environ
    env.update(AAMP_ENV)
    aamp = pexpect.spawn(AAMP_CMD, env=env, timeout=10)

    if args.aamp_log:
        aamp.logfile = sys.stdout.buffer
    elif "logfile" in testdata:
        logfile = open(testdata["logfile"],"wb")
        aamp.logfile = logfile

    #Wait for prompt
    aamp.expect_exact('cmd: ')

    start_time=time.time()

    for e in testdata["expect_list"]:
        if e.get('expect') != None:
            try:
                aamp.expect(e['expect'])
            except pexpect.TIMEOUT:
                print("ERROR TIMEOUT was thrown:",str(aamp))
                test_pass = False
                break
            except:
                print("ERROR Exception was thrown:",str(aamp))
                test_pass = False
                break
            else:
                elapsed = time.time()-start_time
                print("Event {} occurs at elapsed={}".format(str(e).replace("\\", ""),elapsed) )

            finally:
                if (time.time()-start_time) > MAX_TEST_TIME_SECS:
                    print("ERROR Max test time exceeded")
                    test_pass = False
                    break

        if e.get('cmd') != None:
            elapsed = time.time()-start_time
            print("Cmd {} sent at elapsed={}".format(str(e).replace("\\", ""),elapsed) )
            aamp.sendline(e["cmd"])

    #Finish
    aamp.sendline("exit")

    if (args.aamp_log == False) and ("logfile" in testdata):
        logfile.close()

    if test_pass:
        result = "PASSED"
    else:
        result = "FAILED"

    print("{} {}".format(result,testdata["title"]))

    return test_pass
###############################################################################
# Format of TESTDATA:
# * title: Title of the test
# * logfile: File where the log will be written to. The test will verify lines in this log.
# * expect_list:
#   ** cmd: Command that will be sent to aamp.
#   ** expect: The test expects to find a line containing this text in the log.
#
# Example:
# "title": "Title of the test"
# "logfile": "logfilename.txt"
# "expect_list": [
#     {"cmd":"aamp-cli command"},
#     {"expect":"line expected in logfilename.txt"},
# ]
#
# Note:
# This test requires a DASH stream with no subtitles (if it has subtitles, the
# SubtecSimulatorThread starts before the tuned event is received and the test fails).

TESTDATA1= {
"title": "CDAI Single Pipeline - Multiple Assets",
"logfile": "testdata1.txt",
"expect_list": [
   {"cmd":"new"},
   {"expect":"CreateStreamSink Single Pipeline mode, not creating GstPlayer for PLAYER\[1\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Single Pipeline mode, not creating GstPlayer for PLAYER\[2\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Single Pipeline mode, not creating GstPlayer for PLAYER\[3\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Single Pipeline mode, not creating GstPlayer for PLAYER\[4\]"},
   {"cmd":"autoplay"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Single Pipeline mode, resetting current active PLAYER\[0\]"},
   {"expect":"ActivatePlayer Single Pipeline mode, setting active PLAYER\[2\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"detach"},
   {"expect":"DeactivatePlayer Single Pipeline mode, deactivating active PLAYER\[2\]"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Single Pipeline mode, no current active player"},
   {"expect":"ActivatePlayer Single Pipeline mode, setting active PLAYER\[3\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"stop"},
   {"expect":"DeactivatePlayer Single Pipeline mode, asked to deactivate PLAYER\[2\] when current active PLAYER\[3\]"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"detach"},
   {"expect":"DeactivatePlayer Single Pipeline mode, deactivating active PLAYER\[3\]"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Single Pipeline mode, no current active player"},
   {"expect":"ActivatePlayer Single Pipeline mode, setting active PLAYER\[4\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"stop"},
   {"expect":"DeactivatePlayer Single Pipeline mode, asked to deactivate PLAYER\[3\] when current active PLAYER\[4\]"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"seek 10"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"detach"},
   {"expect":"DeactivatePlayer Single Pipeline mode, deactivating active PLAYER\[4\]"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Single Pipeline mode, no current active player"},
   {"expect":"ActivatePlayer Single Pipeline mode, setting active PLAYER\[1\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"stop"},
   {"expect":"DeactivatePlayer Single Pipeline mode, asked to deactivate PLAYER\[4\] when current active PLAYER\[1\]"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"stop"},
   {"expect":"DeactivatePlayer Single Pipeline mode, deactivating and stopping active PLAYER\[1\]"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Single Pipeline mode, no current active player"},
   {"expect":"ActivatePlayer Single Pipeline mode, setting active PLAYER\[1\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"stop"},
   {"expect":"DeactivatePlayer Single Pipeline mode, deactivating and stopping active PLAYER\[1\]"},
]
} 

TESTLIST = [TESTDATA1]

############################################################
parser = argparse.ArgumentParser()
parser.add_argument("--aamp_log", help="Output aamp logging",
                    action="store_true")
args = parser.parse_args()

save_aamp_cfg()
create_aamp_cfg()

for test in TESTLIST:
    if run_test(test) == False:
        restore_aamp_cfg()
        sys.exit(os.EX_SOFTWARE)   #Return non-zero

restore_aamp_cfg()
