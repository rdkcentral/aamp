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
import time
import subprocess
from inspect import getsourcefile


class Simlinear:
    """
    Methods related to starting or stopping simlinear
    """
    def __init__(self, test_dir_path, pytestconfig):
        self.pytestconfig = pytestconfig
        self.test_dir_path = test_dir_path
        self.aamp_home = get_aamp_home()
        self.output_path = os.path.join(self.test_dir_path, "output")
        self.SL_PORT = os.environ["L2_SL_PORT"] if "L2_SL_PORT" in os.environ else '8085'
        self.simlinear_path = os.path.join(self.aamp_home, 'test', 'tools', 'simlinear', 'simlinear.py')

        self.SL_URL = "http://localhost:" + self.SL_PORT + "/"
        self.SL_DATA_PATH = os.path.join(self.test_dir_path, 'testdata')

        self.simlogfile = None  # file object for logging
        self.sl_process = None
        self.paths = None

    def start(self, abr_type, logfile_name):
        """
        Start simlinear web server as a separate process
        """
        if abr_type != 'DASH' and abr_type != 'HLS':
            assert 0, "ERROR unknown abr_type {}".format(abr_type)

        assert os.path.exists(self.simlinear_path), (
            "ERROR File does not exist {} Check setup.".format(self.simlinear_path))

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
        simlinear_cmd = [self.simlinear_path, abr_opt, self.SL_PORT]

        try:
            if self.pytestconfig.config.getoption('sim_log'):
                self.sl_process = subprocess.Popen(simlinear_cmd, cwd=self.SL_DATA_PATH)
            else:
                self.simlogfile = open(os.path.join(self.output_path, logfile_name), "wb")
                self.sl_process = subprocess.Popen(simlinear_cmd, stdout=self.simlogfile, stderr=self.simlogfile,
                                                   cwd=self.SL_DATA_PATH)
        except Exception as e:
            assert 0, "{} gives {}".format(simlinear_cmd, e)

        time.sleep(3)  # Takes time to startup because of listing out manifests! misses URL maybe

    def stop(self):
        """
        Terminate the simlinear process
        """
        if self.sl_process:
            print("Terminating simlinear")
            self.sl_process.terminate()
        if self.simlogfile:
            self.simlogfile.close()

###################################################################################################################


class Aamp:
    """
    Methods related to starting and interacting with aamp-cli
    """

    def __init__(self, pytestconfig=None):
        self.pytestconfig = pytestconfig
        self.aamp_home = get_aamp_home()
        self.AAMP_ENV = {}
        aamp_cli_cmd_prefix = os.environ["AAMP_CLI_CMD_PREFIX"] + ' ' if "AAMP_CLI_CMD_PREFIX" in os.environ else ''
        
        if platform.system() == 'Darwin':
            aamp_cli_path = os.path.join(self.aamp_home, "build", "Debug", "aamp-cli")
        else:
            self.AAMP_ENV.update({"LD_PRELOAD": os.path.join(self.aamp_home, ".libs", "lib", "libdash.so"),
                                  "LD_LIBRARY_PATH": os.path.join(self.aamp_home, ".libs", "lib")})
            aamp_cli_path = os.path.join(self.aamp_home, "build", "aamp-cli")

        assert os.path.exists(aamp_cli_path), "ERROR {} does not exist".format(aamp_cli_path)
        self.AAMP_CMD = '/bin/bash -c "' + aamp_cli_cmd_prefix + aamp_cli_path + '"'

        # Generate a unique logfile name based on current test name
        self.logfile_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0] + ".log"
        self.simlinear = None
        self.output_path = None
        self.aamp_cfg_file = None
        self.test_dir_path = None
        self.aamp_pexpect = None
        self.logfile = None
        self.EXPECT_TIMEOUT = 10

    def run_prerequisite(self, script="prerequisite.sh"):
        """
        Invoke prerequisite.sh if it exists
        """
        path = os.path.join(self.test_dir_path, script)
        prerequisite = os.path.isfile(path)
        if prerequisite is False:
            print("no prerequisites defined {}".format(path))
        else:
            print("Running prerequisite {}".format(path))
            # os.chmod('prerequisite.sh', stat.S_IRWXU)
            ret = subprocess.run('./{}'.format(script), shell=True, cwd=self.test_dir_path)
            assert ret.returncode == 0, "{} failed".format(path)

    def set_paths(self, path_of_test_nnn_py):
        head, tail = os.path.split(path_of_test_nnn_py)
        print("head", head)
        self.test_dir_path = head
        aamp_cfg_dir = self.test_dir_path
        # Set aamp to read aamp.cfg from under l2test so that it does not conflict
        # with $HOME/aamp.cfg that a user may have
        self.AAMP_ENV.update({"AAMP_CFG_DIR": aamp_cfg_dir})
        self.aamp_cfg_file = os.path.join(aamp_cfg_dir, "aamp.cfg")

        self.output_path = os.path.join(self.test_dir_path, "output")
        os.makedirs(self.output_path, exist_ok=True)

        self.run_prerequisite()

    def start_aamp(self):
        """
        Start aamp-cli process and keep connection to stdin stdout via
        pexpect
        """
        env = os.environ
        env.update(self.AAMP_ENV)
        print(self.AAMP_CMD)
        print(self.AAMP_ENV)
        self.aamp_pexpect = pexpect.spawn(self.AAMP_CMD, env=env, timeout=self.EXPECT_TIMEOUT, cwd=self.test_dir_path)

        if self.pytestconfig.config.getoption('aamp_log'):
            self.aamp_pexpect.logfile = sys.stdout.buffer
        else:
            self.logfile = open(os.path.join(self.output_path, self.logfile_name), "wb")
            self.aamp_pexpect.logfile = self.logfile

        try:
            self.aamp_pexpect.expect_exact('cmd: ')
        except Exception as e:
            assert 0, "Cannot start aamp {}".format(e)

    def exit_aamp(self):
        print("Exiting aamp-cli")
        if self.aamp_pexpect:
            try:
                self.sendline("stop")
                self.sendline("exit")
                self.aamp_pexpect.expect(pexpect.EOF)
            except pexpect.EOF:
                pass
            except Exception as e:
                self.aamp_pexpect.kill(9)
                assert 0, "Exception during shutdown of aamp-cli {}".format(e)
        if self.logfile:
            self.logfile.close()

        if self.simlinear:
            self.simlinear.stop()
            self.simlinear = None

    def sendline(self, cmd_line):
        """
        Send a command to aamp-cli
        """
        print("sendline", cmd_line)
        self.aamp_pexpect.sendline(cmd_line)

    def create_aamp_cfg(self, cfg_setting):
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
            if self.pytestconfig.config.getoption('aamp_video') is True:
                pass
            else:
                f.write("useTCPServerSink=true\nTCPServerSinkPort=0\n")
            # Trailing \n can get missed off cfg_setting string which was causing problem if
            # followed by useTCPServerSink
            if cfg_setting:
                f.write(cfg_setting)
            f.close()

        except Exception as e:
            assert 0, "ERROR Exception was thrown".format(e)

    def run_expect_a(self, testdata):
        """
        Provides a simple sequential cmd and expected response structure for test data.
        Format of testdata for run_expect_a()
        "title": "Title of the test"
        "logfile": "log1.txt"              # Optional
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

        if testdata.get('simlinear_type') is not None:
            self.simlinear = Simlinear(self.test_dir_path, self.pytestconfig)
            self.simlinear.start(testdata['simlinear_type'], logfile_name='simlinear_' + self.logfile_name)
        
        if 'logfile' in testdata:
            self.logfile_name = testdata["logfile"]
        print("{} {}".format(testdata["title"],self.logfile_name))

        max_test_time_seconds = testdata.get("max_test_time_seconds", 15)
        self.create_aamp_cfg(testdata.get('aamp_cfg'))

        # start aamp-cli
        self.start_aamp()

        start_time = time.time()

        if self.simlinear:
            self.sendline(self.simlinear.SL_URL+testdata["url"])
        elif testdata.get('url') is not None:
            self.sendline(testdata["url"])

        for idx, e in enumerate(testdata["expect_list"]):
            if e.get('expect') is not None:
                try:
                    self.aamp_pexpect.expect(e['expect'], timeout=max_test_time_seconds)
                except pexpect.TIMEOUT:
                    assert 0, "ERROR TIMEOUT was thrown idx={}:{}".format(idx, e['expect'])
                except Exception as e:
                    assert 0, "ERROR Exception was thrown idx={}:{}".format(idx, str(e))
                else:
                    elapsed = time.time() - start_time
                    match = self.aamp_pexpect.match
                    print("Event idx={} {} occurs at elapsed={}".format
                          (idx, str(e["expect"]).replace("\\", ""), elapsed))
                    if "callback" in e:
                          print("Have callback")
                          e["callback"](match)



                finally:

                    elapsed = time.time() - start_time
                    assert elapsed < (max_test_time_seconds*2), (
                        "ERROR Max test time exceeded elapsed={}".format(elapsed))

            if e.get('cmd') is not None:
                elapsed = time.time() - start_time
                print("Cmd idx={} {} sent at elapsed={}"
                      .format(idx, str(e["cmd"]).replace("\\", ""), elapsed))
                self.sendline(e["cmd"])

    def check_for_missed_events(self, testdata, elapsed, expect_did_happen):
        """
        Check for events that we expect from our test data
        but which have not occurred in the aamp output
        """
        for j in range(len(expect_did_happen)):
            ee = testdata["expect_list"][j]
            if expect_did_happen[j] is False and elapsed > ee["max"] and "not_expected" not in ee:
                assert 0, "ERROR {} never occurred in expected time window".format(ee)

    def run_expect_b(self, testdata):
        """
        Offers more capability for checking logs against expected timing relative to the start of the test.
        Format of testdata for run_expect_b()
        "title": "HLS Audio Discontinuity",
        "logfile": "testdata3.txt",         # Optional
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
        {"expect": r"track[audio] buffering GREEN->YELLOW", "min": 10, "max": 75, "not_expected" : True},
                                            # not_expected used to indicate pattern no expected in this time frame
        {"expect": r"#EXT-X-DISCONTINUITY", "min": 40, "max": 100
                                            "callback" :func
                                            "callback_arg" : 123},
                                            # Allows test to specify a func to call when a match occurs
                                            # In this example results in func(match,123) getting called



         { ... "end_of_test":True}          # End the test cause exit when matching expression encountered
        """

        log_start_timestamp = 0
        log_timestamp = 0
        if 'logfile' in testdata:
            self.logfile_name = testdata["logfile"]
        print("{} {}".format(testdata["title"],self.logfile_name))

        max_test_time_seconds = testdata.get("max_test_time_seconds", 15)

        if testdata.get('simlinear_type'):
            self.simlinear = Simlinear(self.test_dir_path, self.pytestconfig)
            self.simlinear.start(testdata['simlinear_type'], logfile_name='simlinear_' + self.logfile_name)

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
            self.start_aamp()

        # Optional list of commands to give to aamp before starting test proper
        aamp_cmdlist = testdata.get('cmdlist', [])
        for cmd in aamp_cmdlist:
            self.sendline(cmd)
            self.aamp_pexpect.expect('cmd: ')

        expect_re = self.aamp_pexpect.compile_pattern_list(expect_list)

        # Send URL to start playing
        assert 'url' in testdata, "No url specified in test data"

        if self.simlinear:
            self.sendline(self.simlinear.SL_URL+testdata["url"])
        else:
            self.sendline(testdata["url"])

        start_time = time.time()
        end_of_test = False
        while end_of_test is False:
            try:
                i = self.aamp_pexpect.expect_list(expect_re)
            except pexpect.TIMEOUT:
                # Get this exception if no matches for 10secs
                # Use it to do some housekeeping
                # We check that all expected log output has occurred.
                elapsed_since_start = log_timestamp - log_start_timestamp + self.EXPECT_TIMEOUT
                self.check_for_missed_events(testdata, elapsed_since_start, expect_did_happen)

            except Exception as e:
                assert 0, "ERROR Exception was thrown {}".format(e)

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
                            assert 0, "ERROR {} occurred elapsed={}".format(e, elapsed)
                        else:
                            # Event occurred in window and was expected
                            expect_did_happen[i] = True

                            if "callback" in e:
                                print("Have callback")
                                e["callback"](match, e.get("callback_arg"))

                            if "end_of_test" in e:
                                self.check_for_missed_events(testdata, elapsed, expect_did_happen)
                                end_of_test = True

            finally:
                elapsed = time.time()-start_time
                assert elapsed < max_test_time_seconds, "ERROR Max test time exceeded elapsed={}".format(elapsed)


def get_aamp_home():
    """
    Search up directories looking for 'aamp'
    Will not work if somebody renames aamp to something else
    """
    path = os.path.abspath(getsourcefile(lambda: 0))
    while path:
        head, tail = os.path.split(path)
        if tail == "aamp":
            return path
        path = head
