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

import os
import sys
import platform
import pexpect
import argparse
import time
import subprocess
import json

AAMP_HOME = os.environ["AAMP_HOME"] if "AAMP_HOME" in os.environ else os.path.abspath(
        os.path.join('..', '..', '..'))

class Simlinear:
    """
    Methods related to starting or stopping simlinear
    """
    def __init__(self, cmd_line_opts):
        self.args = cmd_line_opts
        self.SL_PORT = os.environ["L2_SL_PORT"] if "L2_SL_PORT" in os.environ else '8085'
        self.SL_PATH = os.path.join(AAMP_HOME, 'test', 'tools', 'simlinear', 'simlinear.py')

        self.SL_URL = "http://localhost:" + self.SL_PORT + "/"
        self.SL_DATA_PATH = 'testdata'
        self.OUTPUT_PATH = os.path.abspath(os.path.join('.','output'))
        os.makedirs(self.OUTPUT_PATH , exist_ok=True)
        self.simlogfile = None  # file object for logging
        self.sl_process = None

    def start(self, abr_type, logfile_name='simlinear_log.txt'):
        """
        Start simlinear web server as a separate process
        """
        if abr_type != 'DASH' and abr_type != 'HLS':
            print("ERROR unknown abr_type ", abr_type)
            sys.exit(os.EX_SOFTWARE)

        if not os.path.exists(self.SL_PATH):
            print("ERROR File does not exist {} Check setup.".format(self.SL_PATH))
            sys.exit(os.EX_SOFTWARE)

        # Kill any existing simlinear process that might have got left running
        # from a previous abnormal exit
        # Unfortunately this will also kill any simlinear running on a different port
        p = subprocess.Popen(['pgrep', '-af', 'simlinear.py'], stdout=subprocess.PIPE)
        out, err = p.communicate()
        if out != b'':
            print("Simlinear already running: -\n" + out.decode("utf-8") + "Attempting to stop it")
            subprocess.run('pgrep -f simlinear.py | xargs -r kill -9', shell=True, executable='/bin/bash')
            time.sleep(1)  # Takes time for cleanup and free of port??

        print("Starting simlinear")
        abr_opt = '--dash' if abr_type == 'DASH' else '--hls'
        simlinear_cmd = [self.SL_PATH, abr_opt, self.SL_PORT]

        try:
            if self.args.sim_log is True:
                self.sl_process = subprocess.Popen(simlinear_cmd, cwd=self.SL_DATA_PATH)
            else:
                self.simlogfile = open(os.path.join(self.OUTPUT_PATH, logfile_name), "wb")
                self.sl_process = subprocess.Popen(simlinear_cmd, stdout=self.simlogfile, stderr=self.simlogfile,
                                                   cwd=self.SL_DATA_PATH)
        except Exception as e:
            print(e)
            sys.exit(os.EX_SOFTWARE)

        time.sleep(3)  # Takes time to startup because of listing out manifests! misses URL maybe

    def stop(self):
        if self.sl_process:
            print("Terminating simlinear")
            self.sl_process.terminate()
        if self.simlogfile:
            self.simlogfile.close()
##################################################################################################################


class Aamp:
    """
    Methods related to starting and interacting with aamp-cli
    """
    def __init__(self, cmd_line_opts):
        self.args = cmd_line_opts

        self.AAMP_ENV = {}
        self.OUTPUT_PATH = os.path.abspath(os.path.join('.', 'output'))
        aamp_cli_cmd_prefix = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''
        os.makedirs(self.OUTPUT_PATH, exist_ok=True)
        if platform.system() == 'Darwin':
            aamp_cli_path = os.path.join(AAMP_HOME, "build", "Debug", "aamp-cli")
        else:
            self.AAMP_ENV.update({"LD_PRELOAD": os.path.join(AAMP_HOME, "Linux", "lib", "libdash.so"),
                                  "LD_LIBRARY_PATH": os.path.join(AAMP_HOME, "Linux", "lib")})
            aamp_cli_path = os.path.join(AAMP_HOME, "Linux", "bin", "aamp-cli")

        self.AAMP_CMD = '/bin/bash -c "' + aamp_cli_cmd_prefix + aamp_cli_path + '"'

        # Set aamp to read aamp.cfg from under l2test so that it does not conflict
        # with $HOME/aamp.cfg that a user may have
        tes_path = os.path.abspath(os.path.join('.'))
        aamp_cfg_dir = tes_path
        self.AAMP_ENV.update({"AAMP_CFG_DIR": aamp_cfg_dir})
        self.aamp_cfg_file = os.path.join(aamp_cfg_dir, "aamp.cfg")

        self.aamp_pexpect = None
        self.logfile = None
        self.EXPECT_TIMEOUT = 10

    def start_aamp(self, logfile_name='logfile.log'):
        env = os.environ
        env.update(self.AAMP_ENV)
        print(self.AAMP_CMD)
        print(self.AAMP_ENV)
        self.aamp_pexpect = pexpect.spawn(self.AAMP_CMD, env=env, timeout=self.EXPECT_TIMEOUT)

        if self.args.aamp_log:
            self.aamp_pexpect.logfile = sys.stdout.buffer
        else:
            self.logfile = open(os.path.join(self.OUTPUT_PATH, logfile_name), "wb")
            self.aamp_pexpect.logfile = self.logfile

        self.aamp_pexpect.expect_exact('cmd: ')

    def exit_aamp(self):
        print("Exiting aamp-cli")
        try:
            self.sendline("stop")
            self.sendline("exit")
            self.aamp_pexpect.sendeof()
        except Exception as e:
            print("Exception during shutdown of aamp-cli", e)
            self.aamp_pexpect.kill(9)
        if self.logfile:
            self.logfile.close()

    def sendline(self, cmd_line):
        print("sendline", cmd_line)
        self.aamp_pexpect.sendline(cmd_line)

    def create_aamp_cfg(self,cfg_setting):
        """
        aamp.cfg should contain
        info=true
        trace=true
        Otherwise aamp-cli will not output the logging required for test validation.
        useTCPServerSink=true See RDKAAMP-48
        """
        try:
            print("Creating", self.aamp_cfg_file)
            f = open(self.aamp_cfg_file, "w")
            if cfg_setting:
                f.write(cfg_setting)
            if self.args.aamp_video is False:
                f.write("useTCPServerSink=true\n")
            f.close()

        except Exception as e:
            print("ERROR Exception was thrown", e)
            sys.exit(os.EX_SOFTWARE)

    def run_expect_a(self, testdata):
        """
        Provides a simple sequential cmd and expected response structure for test data.
        Format of testdata for run_expect_a()
        "title": "Title of the test"
        "logfile": "log1.txt"              # name to use for logfile
        "max_test_time_seconds": 30        # Optional, The max time the test is allowed to run before fail, default 15
        "aamp_cfg": "info=true\ntrace=true\n", # Values to set in aamp.cfg
        "expect_list": [                   # Simple list with cmds to send to aamp or log lines to
                                            # expected back, no timing information
             {"cmd":"aamp-cli command"},
             {"expect":"line expected from aamp"},
             {"cmd":"aamp-cli command"},
             {"expect":"line expected from aamp"},
        ]
        """
        test_pass = True
        print("{} {}".format(testdata["title"], testdata.get("logfile", '')))

        max_test_time_seconds = testdata.get("max_test_time_seconds", 15)
        self.create_aamp_cfg(testdata.get('aamp_cfg'))

        # start aamp-cli
        self.start_aamp()

        start_time = time.time()

        for idx, e in enumerate(testdata["expect_list"]):
            if e.get('expect') is not None:
                try:
                    self.aamp_pexpect.expect(e['expect'],timeout=max_test_time_seconds)
                except pexpect.TIMEOUT:
                    print("ERROR TIMEOUT was thrown idx={}:{}".format(idx, str(self.aamp_pexpect)))
                    test_pass = False
                    break
                except Exception as e:
                    print("ERROR Exception was thrown idx={}:{}".format(idx, str(e)))
                    test_pass = False
                    break
                else:
                    elapsed = time.time() - start_time
                    print("Event idx={} {} occurs at elapsed={}".format
                          (idx, str(e["expect"]).replace("\\", ""), elapsed))

                finally:
                    if (time.time() - start_time) > max_test_time_seconds:
                        print("ERROR Max test time exceeded")
                        test_pass = False
                        break

            if e.get('cmd') is not None:
                elapsed = time.time() - start_time
                print("Cmd idx={} {} sent at elapsed={}".format(idx, str(e["cmd"]).replace("\\", ""), elapsed))
                self.sendline(e["cmd"])

        # Finish
        self.exit_aamp()

        if test_pass:
            result = "PASSED"
        else:
            result = "FAILED"

        print("{} {}".format(result, testdata["title"]))

        return test_pass

    def check_for_missed_events(self, testdata, elapsed, expect_did_happen):
        """
        Check for events that we expect from our test data
        but which have not occurred in the aamp output
        """
        for j in range(len(expect_did_happen)):
            ee = testdata["expect_list"][j]
            if expect_did_happen[j] is False and elapsed > ee["max"] and "not_expected" not in ee:
                print("ERROR {} never occurred in expected time window".format(ee))
                return False
        return True

    def run_expect_b(self, testdata):
        """
        Offers more capability for checking logs against expected timing relative to the start of the test.
        Format of testdata for run_expect_b()
        "title": "HLS Audio Discontinuity",
        "logfile": "testdata3.txt",         # Optional, ensures log files from successive test in do not overwrite
        "max_test_time_seconds": 15         # Optional, The max time the test is allowed to run before fail
        "aamp_cfg": "info=true\ntrace=true\n", # Values to set in aamp.cfg
        "cmdlist": [                        # Optional, list of commands to give to aamp-cli before starting test proper
                                            # sent before the url
            "setconfig {"logMetadata":true,"client-dai":true",
            "advert add " + AD_URLS[3] + " 30"
        ]
        "url": "m3u8s_audio_discontinuity_180s/manifest.1.m3u8",
                                            # Partial url if simlinear is used, full url otherwise. Sent to aamp to
                                            # start the test
        "simlinear_type": "HLS",            # Optional, Start simlinear to serve url, Specify abr type "HLS" or  "DASH"

        "expect_list": [

        {"expect": r"Video Profile added to ABR", "min": 0, "max": 1},
                                            # List of regular expressions to expect
                                            # with min max when it is expected to occur
                                            # The first expect in the list is used to set the time reference to 0
        {"expect": r"track\[audio\] buffering GREEN->YELLOW", "min": 10, "max": 75, "not_expected" : True},
                                            # not_expected used to indicate pattern no expected in this time frame
        {"expect": r"#EXT-X-DISCONTINUITY", "min": 40, "max": 100},



         { ... "end_of_test":True}          # End the test cause exit when matching expression encountered
        """

        test_pass = True
        log_start_timestamp = 0
        log_timestamp = 0
        simlinear = None

        print("{} {}".format(testdata["title"], testdata.get("logfile", '')))
        max_test_time_seconds = testdata.get("max_test_time_seconds", 15)

        if testdata['simlinear_type']:
            simlinear = Simlinear(self.args)
            simlinear.start(testdata['simlinear_type'], logfile_name='simlinear_' + testdata['logfile'])

        expect_list = []
        expect_did_happen = []
        # Build list of expected strings from testdata
        for e in testdata["expect_list"]:
            expect_list.append(e["expect"])
            expect_did_happen.append(False)

        # Add a pattern which matches on the timestamp at the beginning of each log line
        expect_list.append(r"\n(\d{10})")

        # start aamp-cli
        self.create_aamp_cfg(testdata.get('aamp_cfg'))

        # A test can start aamp early to giv it some commands, in which case no need to start here
        if self.aamp_pexpect is None:
            self.start_aamp('aamp_' + testdata['logfile'])

        # Optional list of commands to give to aamp before starting test proper
        aamp_cmdlist = testdata.get('cmdlist', [])
        for cmd in aamp_cmdlist:
            self.sendline(cmd)
            self.aamp_pexpect.expect('cmd: ')

        expect_re = self.aamp_pexpect.compile_pattern_list(expect_list)

        # Send URL to start playing
        if simlinear:
            self.sendline(simlinear.SL_URL+testdata["url"])
        else:
            self.sendline(testdata["url"])

        start_time = time.time()
        end_of_test = False
        while end_of_test is False and test_pass is True:
            try:
                i = self.aamp_pexpect.expect_list(expect_re)
            except pexpect.TIMEOUT:
                # Get this exception if no matches for 10secs
                # Use it to do some housekeeping
                # We check that all expected log output has occurred.
                elapsed_since_start = log_timestamp - log_start_timestamp + self.EXPECT_TIMEOUT
                if self.check_for_missed_events(testdata, elapsed_since_start, expect_did_happen) is False:
                    test_pass = False

            except Exception as e:
                print("ERROR Exception was thrown:", e)
                test_pass = False
                end_of_test = True
                break
            else:
                if i == len(expect_list)-1:
                    # last entry in our list which is the timestamp pattern
                    # print(aamp.match.group(0))
                    # print(aamp.match.group(1))
                    log_timestamp = int(self.aamp_pexpect.match.group(1))
                else:
                    # First match in list , set time reference
                    if i == 0 and log_start_timestamp == 0:
                        log_start_timestamp = log_timestamp

                    elapsed = log_timestamp - log_start_timestamp
                    # Get details of the event we just received
                    e = testdata["expect_list"][i]
                    match = self.aamp_pexpect.match
                    print("Event {} occurs at elapsed={}".format(match.group(0), elapsed))

                    if elapsed >= e["min"] and elapsed <= e["max"]:
                        if "not_expected" in e:
                            # We got event within a time window when we were not expecting it
                            print("ERROR {} occurred elapsed={}".format(e, elapsed))
                            test_pass = False
                        else:
                            # Event occurred in window and was expected
                            expect_did_happen[i] = True

                            if "end_of_test" in e:
                                if self.check_for_missed_events(testdata, elapsed, expect_did_happen) is False:
                                    test_pass = False
                                end_of_test = True

            finally:
                if (time.time()-start_time) > max_test_time_seconds:
                    print("ERROR Max test time exceeded")
                    test_pass = False
                    end_of_test = True

        # Finish
        self.exit_aamp()

        if test_pass:
            result = "PASSED"
        else:
            result = "FAILED"

        print("{} {}".format(result, testdata["title"]))

        if simlinear:
            simlinear.stop()

        return test_pass


def parse_cmd_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--aamp_log", help="Output aamp logging",
                        action="store_true")
    parser.add_argument("--sim_log", help="Output sim logging",
                        action="store_true")
    parser.add_argument("-v", "--aamp_video", help="Run AAMP with video window, but no A/V gap detection",
                        action="store_true")
    parser.add_argument("--ignore_fails",
                        help="Continue with next test even if previous failed. Default option is to exit on failure",
                        action="store_true")
    parser.add_argument('--repeat', type=int, default=1, help='Repeat the set of tests n times')

    return parser.parse_args()
