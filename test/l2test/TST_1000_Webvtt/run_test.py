#!/usr/bin/env python3
#Starts aamp-cli and initiates playback by giving it a stream URL
#verifys aamp log output against expected list of events

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

AAMP_ENV = {"LD_PRELOAD": os.path.join(AAMP_HOME, "Linux", "lib", "libdash.so"), "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, "Linux", "lib")}
AAMP_CMD = os.path.join(AAMP_HOME, "Linux", "bin", "aamp-cli")

MAX_TEST_TIME_SECS = 15

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
            f.write("info=true\ntrace=true\nuseTCPServerSink=true\n")
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
"title": "Setting WebVTT font size",
"logfile": "testdata1.txt",
"expect_list": [
    {"cmd":"set subtecSimulator 1"},
    {"expect":"SubtecSimulatorThread - listening for packets",},
    {"cmd":"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd"},
    {"expect":"AAMP_EVENT_TUNED"},
    {"cmd":"set textTrack data/test.vtt"},
    {"expect":"webvtt data received from application",},
    {"cmd":"set ccStyle data/small.json"},
    {"expect":"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect":"Calling SubtitleParser"},
    {"expect":"\*\*\*SubtecSimulatorThread:"},
    {"expect":"Type:CC_SET_ATTRIBUTE"},
    {"expect":"attribute\[5\]: 0"},
    {"cmd":"set ccStyle data/medium.json"},
    {"expect":"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect":"Calling SubtitleParser"},
    {"expect":"\*\*\*SubtecSimulatorThread:"},
    {"expect":"Type:CC_SET_ATTRIBUTE"},
    {"expect":"attribute\[5\]: 1"},
    {"cmd":"set ccStyle data/large.json"},
    {"expect":"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect":"Calling SubtitleParser"},
    {"expect":"\*\*\*SubtecSimulatorThread:"},
    {"expect":"Type:CC_SET_ATTRIBUTE"},
    {"expect":"attribute\[5\]: 2"},
    {"cmd":"set ccStyle data/extra_large.json"},
    {"expect":"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect":"Calling SubtitleParser"},
    {"expect":"\*\*\*SubtecSimulatorThread:"},
    {"expect":"Type:CC_SET_ATTRIBUTE"},
    {"expect":"attribute\[5\]: 3"},
    {"cmd":"set ccStyle data/auto.json"},
    {"expect":"Calling StreamAbstractionAAMP::SetTextStyle"},
    {"expect":"Calling SubtitleParser"},
    {"expect":"\*\*\*SubtecSimulatorThread:"},
    {"expect":"Type:CC_SET_ATTRIBUTE"},
    {"expect":"attribute\[5\]: 4294967295"},
    {"cmd":"stop"},
    {"cmd":"set subtecSimulator 0"},
    {"expect":"SubtecSimulatorThread - exit",},
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
