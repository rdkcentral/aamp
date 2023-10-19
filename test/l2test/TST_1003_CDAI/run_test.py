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
# verifies aamp log output against expected list of events

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

AAMP_CMD = '/bin/bash -c "' + AAMP_CLI_CMD_PREFIX + AAMP_CLI_PATH + '"'


MAX_TEST_TIME_SECS = 60
OUTPUT_PATH = 'output'

aamp_cfg_saved_file = ""
aamp_cfg_file = os.path.join(AAMP_CFG_DIR, "aamp.cfg")
print("AAMP_CFG_DIR=" + AAMP_CFG_DIR)

cmdlogfile = None
aamp_cli = None

def stop_and_exit(code):
    sys.exit(code)

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
            print("Creating",aamp_cfg_file)
            f = open(aamp_cfg_file, "w")
#            f.write("info=true\ntrace=true\nuseTCPServerSink=true\n")
            f.write("info=true\ntrace=true\nuseTCPServerSink=false\n")
            if args.aamp_video == False:
               f.write("useTCPServerSink=true\n")
            f.close()
        else:
            print("HOME env not set")
            stop_and_exit(os.EX_SOFTWARE)
    except Exception as e:
        print("ERROR Exception was thrown in create_aamp_cfg(): %s" % (e))
        stop_and_exit(os.EX_SOFTWARE)

def aamp_sendline(cmd_line):
    global cmdlogfile
    global aamp_cli

    if (cmdlogfile != None):
        cmdlogfile.write(cmd_line + "\n")

    aamp_cli.sendline(cmd_line)

#####################################################################
def run_test(testdata):
    global args
    global cmdlogfile
    global aamp_cli

    test_pass=True
    print("{} {}".format(testdata["title"],testdata["logfile"]))

    #start aamp-cli

    env = os.environ
    env.update(AAMP_ENV)
    print(AAMP_CMD)
    aamp = pexpect.spawn(AAMP_CMD, env=env, timeout=10)
    aamp_cli = aamp

    logfile_name = os.path.join(OUTPUT_PATH, testdata["logfile"])

    if args.aamp_log:
        aamp.logfile = sys.stdout.buffer
    elif "logfile" in testdata:
        logfile = open(logfile_name,"wb")
        aamp.logfile = logfile

    cmdlogfile_name = os.path.join(OUTPUT_PATH, Path(testdata["logfile"]).parent, Path(testdata["logfile"]).stem) + "_cmd.txt"
    cmdlogfile = open(cmdlogfile_name,"w")

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
                print("Event {} occurs at elapsed={}".format(str(e["expect"]).replace("\\", ""),elapsed) )

            finally:
                if (time.time()-start_time) > MAX_TEST_TIME_SECS:
                    print("ERROR Max test time exceeded")
                    test_pass = False
                    break

        if e.get('cmd') != None:
            elapsed = time.time()-start_time
            print("Cmd {} sent at elapsed={}".format(str(e["cmd"]).replace("\\", ""),elapsed) )
            aamp_sendline(e["cmd"])

    #Finish
    aamp_sendline("exit")

    if (args.aamp_log == False) and ("logfile" in testdata):
        logfile.close()

    aamp_cli = None
    cmdlogfile.close()

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
"title": "CDAI Multi Pipeline - Multiple Assets",
"logfile": "testdata1.txt",
"expect_list": [
   {"cmd":"new"},
   {"expect":"CreateStreamSink Undefined Pipeline mode, creating GstPlayer for PLAYER\[1\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Undefined Pipeline mode, creating GstPlayer for PLAYER\[2\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Undefined Pipeline mode, creating GstPlayer for PLAYER\[3\]"},
   {"cmd":"new"},
   {"expect":"CreateStreamSink Undefined Pipeline mode, creating GstPlayer for PLAYER\[4\]"},
   {"cmd":"autoplay"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Undefined Pipeline, forcing to Multi Pipeline PLAYER\[2\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"detach"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Multi Pipeline mode, do nothing PLAYER\[3\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 2"},
   {"expect":"selected player 2"},
   {"cmd":"stop"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"detach"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Multi Pipeline mode, do nothing PLAYER\[4\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 3"},
   {"expect":"selected player 3"},
   {"cmd":"stop"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"seek 10"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"detach"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Multi Pipeline mode, do nothing PLAYER\[1\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"select 4"},
   {"expect":"selected player 4"},
   {"cmd":"stop"},
   {"cmd":"select 1"},
   {"expect":"selected player 1"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"stop"},
   {"cmd":"https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd"},
   {"cmd":"play"},
   {"expect":"ActivatePlayer Multi Pipeline mode, do nothing PLAYER\[1\]"},
   {"expect":"NotifyFirstBufferProcessed"},
   {"cmd":"sleep 3000"},
   {"expect":"sleep complete"},
   {"cmd":"stop"},
]
}

TESTLIST = [TESTDATA1]

############################################################
parser = argparse.ArgumentParser()
parser.add_argument("--aamp_log", help="Output aamp logging",
                    action="store_true")
parser.add_argument("-v","--aamp_video", help="Run AAMP with video window, but no A/V gap detection",
                    action="store_true")
args = parser.parse_args()

if args.aamp_video:
    print("AAMP with video window option selected. There will be no A/V gap detection")

if not os.path.exists(AAMP_CLI_PATH):
    print("ERROR cannot access AAMP_CLI_PATH={}".format(AAMP_CLI_PATH))
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

try:
    create_aamp_cfg()

    for test in TESTLIST:
        if run_test(test) == False:
            stop_and_exit(os.EX_SOFTWARE)   #Return non-zero

    stop_and_exit(0)

except Exception as e:
    print("ERROR Exception was thrown: %s" % (e))
    stop_and_exit(os.EX_SOFTWARE)   #Return non-zero

