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
import hashlib
import requests
import shutil
import dateutil.parser
import datetime
import re

class Simlinear:
    """
    Methods related to starting or stopping simlinear
    """
    def __init__(self, logfile_path, testdata_path,  extra_args):
        """
        logfile_path    -  path where output can be written exists E.G .../l2test/TST_2001/output
        testdata_path   -  path where data to be served by simlinear is located .../testdata 
        extra_args       - DASH or HLS
        """
        self.extra_args = extra_args
        self.aamp_home = get_aamp_home()
        self.SL_PORT = os.environ["L2_SL_PORT"] if "L2_SL_PORT" in os.environ else '8085'
        self.simlinear_path = os.path.join(self.aamp_home, 'test', 'tools', 'simlinear', 'simlinear.py')

        self.SL_URL = "http://localhost:" + self.SL_PORT + "/"
        self.SL_DATA_PATH = testdata_path
        self.logfile_path = logfile_path
        self.simlogfile = None  # file object for logging
        self.sl_process = None
        self.paths = None

    def start(self):
        """
        Start simlinear web server as a separate process
        """
        if self.extra_args != 'DASH' and self.extra_args != 'HLS':
            assert 0, "ERROR unknown abr_type {}".format(self.extra_args)

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
        abr_opt = '--dash' if self.extra_args == 'DASH' else '--hls'
        simlinear_cmd = [self.simlinear_path, abr_opt, self.SL_PORT]

        try:
            self.simlogfile = open(self.logfile_path, "wb")
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
class ArchiveFetch:
    """
    Methods related to fetching stream archives off remote server and unpacking locally so they
    can be used by some server
    """
    def __init__(self, archive_path):
        """
        archive_path - directory where archives can be written
        """
        self.archive_path = archive_path

    def download_unpack_file(self, url, local_archive_path, testdata_path):
        """
        url                 - url of archive to fetch
        local_archive_path  - file where packed archive can be written
        testdata_path       - directory where to unpack the archive
        """

        print(f"Downloading {url}")
        try:
            response = requests.get(url, stream=True)
            assert response.ok, f"Cannot fetch {url} {response.reason}"
            with open(local_archive_path, 'wb') as f:
                shutil.copyfileobj(response.raw, f)
            print("File downloaded successfully!")
        except requests.exceptions.RequestException as e:
            print("Error downloading the file:", e)

        # Delete any existing and unpack new archive
        if os.path.exists(testdata_path):
            shutil.rmtree(testdata_path)
        os.makedirs(testdata_path, exist_ok=True)
        ext = os.path.splitext(local_archive_path)[1]
        cmd = None
        if ext == '.zip':
            cmd = ['unzip', '-q', local_archive_path, '-d' , testdata_path]
        else:
            cmd = ['tar','-xf', local_archive_path, '-C' , testdata_path]
        print(cmd)
        result = subprocess.run(cmd)
        assert result.returncode == 0, f"Unpack failed {cmd}"

    def url_to_hash(self, url):
        """
        Convert url into unique directory name
        """
        return hashlib.md5(url.encode('utf-8')).hexdigest()

    def fetch_archive(self, archive_url):
        print(f'fetch_archive {archive_url}')

        """ 
        With users running l2 test locally then the downloaded 
        archive may be stored in their local repo for some time and 
        become out of date if a new version of the archive is uploaded to the server.
        
        check_for_newer_archive_on_server causes server to be checked for newer archive. 
        """
        check_for_newer_archive_on_server = True
        path = None
        need_download = False

        url_hash = self.url_to_hash(archive_url)
        path = os.path.join(self.archive_path, url_hash)
        testdata_path = os.path.join(path,'testdata')
        local_filename = archive_url.split('/')[-1]
        local_archive_path = os.path.join(path,local_filename)

        if check_for_newer_archive_on_server:
            r = requests.head(archive_url)
            assert r.ok, f"Cannot fetch {archive_url} {r.reason}"
            url_time = r.headers['last-modified']
            url_date = dateutil.parser.parse(url_time)

        if not os.path.exists(testdata_path):
            os.makedirs(path, exist_ok=True)
            need_download = True
        else:
            # Have local copy of archive, check if newer version on server
            if check_for_newer_archive_on_server:
                archive_date = datetime.datetime.fromtimestamp(os.path.getmtime(local_archive_path),datetime.timezone.utc)
                print(f"archive_date={archive_date} vs url_date={url_date}")
                time_diff = url_date - archive_date
                if abs(time_diff.total_seconds()) >60 :
                    need_download = True

        if need_download:
            self.download_unpack_file(archive_url, local_archive_path, testdata_path)
            if check_for_newer_archive_on_server:
                # Set downloaded file to have same date time as on server so we can tell when server version changes
                since_epoch = url_date.timestamp()
                os.utime(local_archive_path, (since_epoch, since_epoch))

            # Write url to local file so we know where the archive came from
            info_path = os.path.join(path,'info.txt')
            with open(info_path, 'w') as f:
                f.write(f"{archive_url}\n")

        return testdata_path

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

        # self.simlinear could be merged into self.archive_server at some later date
        self.simlinear = None
        self.archive_server = None     # Class to start and stop testdata server
        self.output_path = None        # Where to write logs for specific test .../l2test/TEST_nnn/output
        self.testdata_path = None      # Where unpacked archive has been put  
                                       # .../l2test/TEST_nnn/testdata or download_archive/xxxx/testdata
        self.aamp_cfg_file = None
        self.test_dir_path = None      # Directory of currently running test .../l2test/TEST_nnn
        self.aamp_pexpect = None
        self.logfile = None
        self.l2test_path = None        # .../aamp/test/l2test
        self.EXPECT_TIMEOUT = 30
        self.max_test_time_seconds = 0

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
        self.l2test_path = os.path.dirname(head)

        aamp_cfg_dir = self.test_dir_path
        # Set aamp to read aamp.cfg from under l2test so that it does not conflict
        # with $HOME/aamp.cfg that a user may have
        self.AAMP_ENV.update({"AAMP_CFG_DIR": aamp_cfg_dir})
        self.aamp_cfg_file = os.path.join(aamp_cfg_dir, "aamp.cfg")

        self.output_path = os.path.join(self.test_dir_path, "output")
        os.makedirs(self.output_path, exist_ok=True)

        # Set default path to be when archive is downloaded by prerequisite.sh
        self.testdata_path = os.path.join(self.test_dir_path, 'testdata')

        """
        default archive path is .../aamp/test/l2test/download_archive
        but can be set with env var
        """
        self.archive_path = os.environ.get('L2_ARCHIVE_PATH',os.path.join(self.l2test_path,'download_archive'))

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

        if self.archive_server:
            self.archive_server.stop()
            self.archive_server = None

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

    def common_startup(self,testdata):
        """
        startup that is common to both expect_a() expect_b()
        """

        if 'logfile' in testdata:
            self.logfile_name = testdata["logfile"]
        print("{} {}".format(testdata["title"],self.logfile_name))

        self.max_test_time_seconds = testdata.get("max_test_time_seconds", 15)

        assert not ('archive_server' in testdata and 'simlinear_type' in testdata), \
            "Cannot have both archive_server' and  'simlinear_type' in TESTDATA"

        if 'archive_url' in testdata:
            o = ArchiveFetch(self.archive_path)
            self.testdata_path = o.fetch_archive(testdata.get('archive_url'))

        server_logfile_path = os.path.join(self.output_path,'server_' + self.logfile_name)
        if 'archive_server' in testdata:
            svr=testdata['archive_server']
            """
            The server class is expected to take these arguments
            output_path - path to dir where logs can be written
            data_root """
            self.archive_server = svr['server_class'](server_logfile_path,self.testdata_path,svr.get('extra_args'))
            assert self.archive_server.start(), f"Could not start server {svr}"

        if testdata.get('simlinear_type'):
            self.simlinear = Simlinear(server_logfile_path, self.testdata_path,testdata['simlinear_type'])
            self.simlinear.start()

        self.create_aamp_cfg(testdata.get('aamp_cfg'))

        # start aamp-cli
        self.start_aamp()

        # Optional list of commands to give to aamp before starting test proper
        aamp_cmdlist = testdata.get('cmdlist', [])
        for cmd in aamp_cmdlist:
            self.sendline(cmd)
            self.aamp_pexpect.expect('cmd: ')

        url = testdata.get('url')
        if self.simlinear and url is not None:
            assert "http" not in url, f'url parameter cannot start with http for simlinear {url}'
            self.sendline(self.simlinear.SL_URL+url)
        elif url is not None:
            assert "http" in url, f'url parameter should start with http {url}'
            self.sendline(url)

    def run_expect_a(self, testdata):
        """
        Provides a simple sequential cmd and expected response structure for test data.
        Format of testdata for run_expect_a()
        "title": "Title of the test"
        "logfile": "log1.txt"              # Optional, default let framework set logfile name
        "max_test_time_seconds": 30        # Optional, default 15. Fail if waiting for match on next expect exceeds this.
        "aamp_cfg": "info=true\ntrace=true\n", # Values to set in aamp.cfg

        One of the following to provide a stream for aamp to play
        1) Fetch archive and play with simlinear
        "archive_url" : "https://cpetestutility.stb.r53.xcal.tv/skyatlantic-30t-2.tgz",
        "url": "v1/frag/bmff/enc/cenc/t/SKYATHD_HD_SU_SKYUK_4053_0_6139857640084951163.mpd", #path from unpacked contents of .tgz
        'simlinear_type': 'DASH',            # Specify abr type "HLS" or  "DASH"
        2) Fetch archive and serve from some other custom server 
        "archive_url": "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/content.tar.xz",
        "archive_server": {'server_class': WindowServer},
        "url": "http://localhost:8080/content/main.mpd?live=true",
        3) Play from external server, no archive involved
        "url": "https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/ad-insertion-testcase1.mpd",

        "cmdlist": [                        # Optional, list of commands to give to aamp-cli before starting test proper
            'setconfig {"logMetadata":true,"client-dai":true}',
        ]
        "expect_list": [                    # Simple list with cmds to send to aamp or log lines to
                                            # expected back, no timing information
             {"cmd":"aamp-cli command"},
             {"expect":"re expected from aamp"},
             {"cmd":"aamp-cli command"},
             {"expect":"line expected from aamp"},
             {"expect":"some value ([12345]+)", "callback" :func, "callback_arg" : 123},
                                            # Allows test to specify a func to call when a match occurs
                                            # In this example results in func(match,123) getting called
                                            # match value corresponds to obj returned from re.search()
             {"expect": re.escape("value [qwerty]"})
                                            # Special characters in string are escaped so it is not 
                                            # interpreted as re
        ]
        """

        self.common_startup(testdata)

        line_compiled = re.compile(".*?\r\n")
        start_time = time.time()
        for idx, e in enumerate(testdata["expect_list"]):
            if e.get('expect') is not None:
                match = None
                start_match_time = time.time()
                while match is None:
                    try:
                        self.aamp_pexpect.expect(line_compiled)
                    except pexpect.TIMEOUT:
                        assert 0, "ERROR TIMEOUT was thrown idx={}:{}".format(idx, e['expect'])
                    except Exception as e:
                        assert 0, "ERROR Exception was thrown idx={}:{}".format(idx, str(e))
                    else:
                        log_line = self.aamp_pexpect.match.group(0).decode('utf-8')
                        match = re.search(e['expect'],log_line)
                        if match:
                            elapsed = time.time() - start_time
                            stripped = str(e["expect"]).replace("\\", "")
                            print(f"Event idx={idx} {stripped} occurs at elapsed={elapsed:.2f}")
                            if "callback" in e:
                                print("Have callback")
                                e["callback"](match)

                    finally:
                        pass

                    match_elapsed = time.time() - start_match_time
                    assert match_elapsed < self.max_test_time_seconds, (f"ERROR Max test time exceeded elapsed={match_elapsed:.2f} {e}")

            if e.get('cmd') is not None:
                elapsed = time.time() - start_time
                cmd = e["cmd"]
                print(f"Cmd idx={idx} {cmd} sent at elapsed={elapsed:.2f}")
                self.sendline(cmd)

    def check_for_missed_events(self, testdata, elapsed, expect_did_happen):
        """
        Check for events that we expect from our test data
        but which have not occurred in the aamp output
        """
        for j in range(len(expect_did_happen)):
            ee = testdata["expect_list"][j]
            if expect_did_happen[j] is False and elapsed > ee.get("max",self.max_test_time_seconds) and "not_expected" not in ee:
                assert 0, "ERROR {} never occurred in expected time window".format(ee)

    def run_expect_b(self, testdata):
        """
        Offers more capability for checking logs against expected timing relative to the start of the test.
        Format of testdata for run_expect_b()
        "title": "HLS Audio Discontinuity",
        "logfile": "testdata3.txt",         # See expect_a documentation comments
        "max_test_time_seconds": 15         # Optional, default 15. Fail if the test running time exceeds this.
        "aamp_cfg": "info=true\ntrace=true\n", # See expect_a documentation comments
        "cmdlist": [                        # See expect_a documentation comments
        ....
        ]
        "url": "m3u8s_audio_discontinuity_180s/manifest.1.m3u8",
                                            # See expect_a documentation comments
        "simlinear_type": "HLS",            # See expect_a documentation comments

        "expect_list": [                    # Formatt specific to expect_b
                                            # List of regular expressions to expect, look for them all simultaneously:

        {"expect": re.escape("track[audio] buffering GREEN->YELLOW") ..}
                                            # Log line match, See expect_a documentation comments

        {"expect": "expected in first 10S" , "min": 0, "max": 10},
        {"expect": "expected in 2nd 10S" ,   "min": 10, "max": 20},
                                            # optional min max values in seconds give time window when log line
                                            # is expected to occur.
                                            # min defaults to 0, max defaults to max_test_time_seconds

        {"expect": ...                      "not_expected" : True},
                                            # not_expected used to indicate pattern not expected in this time window

        {"expect": r"#EXT-X-DISCONTINUITY", "callback": func, "callback_arg": "play"},
                                            # Also see expect_a documentation for callback
                                            # func() can be written to send the argument to aamp-cli

        {"expect": r"#EXT-X-DISCONTINUITY", "callback_once": func, "callback_arg": "play"},
                                            # Same as callback, but if the expect line if matched multiple times,
                                            # the callback will only be called once, the first time it is matched

         { ... "end_of_test":True}          # End the test cause exit when matching expression encountered
         ]                                  # End of expect_list
        """

        self.common_startup(testdata)

        expect_list_compiled = []
        expect_did_happen = []
        # Build list of expected strings from testdata
        for e in testdata["expect_list"]:
            expect_list_compiled.append(re.compile(e["expect"]))
            expect_did_happen.append(False)

        # Send URL to start playing
        assert 'url' in testdata, "No url specified in test data"

        start_time = time.time()
        end_of_test = False
        last_missed_event_check = 0
        line_compiled = re.compile(".*?\r\n")
        while end_of_test is False:
            elapsed = time.time() - start_time
            try:
                i = self.aamp_pexpect.expect(line_compiled)
            except pexpect.TIMEOUT:
                # Get here if no log lines received for the default pexpect time
                # do nothing as the test will time out depending on it own setting
                pass

            except Exception as e:
                assert 0, "ERROR Exception was thrown {}".format(e)

            else:
                log_line = self.aamp_pexpect.match.group(0).decode('utf-8')
                #print("log_line ",log_line)
                for i, compiled_re in enumerate(expect_list_compiled):
                    match = compiled_re.search(log_line)
                    if match:
                        # Get details of the event we just received
                        e = testdata["expect_list"][i]
                        print("Event({}) {} occurs at elapsed={}".format(i, match.group(0), int(elapsed)))

                        if elapsed >= e.get("min",0) and elapsed <= e.get("max",self.max_test_time_seconds):
                            if "not_expected" in e:
                                # We got event within a time window when we were not expecting it
                                assert 0, "ERROR {} occurred elapsed={}".format(e, elapsed)
                            else:
                                if "callback_once" in e:
                                    if expect_did_happen[i] is False:
                                        print("Have one-time callback")
                                        e["callback_once"](match, e.get("callback_arg"))
                                elif "callback" in e:
                                    print("Have callback")
                                    e["callback"](match, e.get("callback_arg"))

                                # Event occurred in window and was expected
                                expect_did_happen[i] = True

                                if "end_of_test" in e:
                                    self.check_for_missed_events(testdata, elapsed, expect_did_happen)
                                    end_of_test = True

                if int(elapsed) != last_missed_event_check:
                    # Every second check if we have not received any expected events
                    last_missed_event_check = int(elapsed)
                    self.check_for_missed_events(testdata, elapsed, expect_did_happen)

            finally:
                assert elapsed < self.max_test_time_seconds, "ERROR Max test time exceeded elapsed={}".format(elapsed)


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
