#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia Ltd.
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
import sys,os
base_dir = os.path.dirname(sys.argv[0])
sys.path.append(base_dir + "/../library")

from datetime import *
import time
import re
import argparse
import threading
import subprocess
import queue
import signal
import io
import json
from enum import Enum
from pathlib import Path
from http.client import *
from hls_manifests import *
from dash_manifests import *

HELP = """
This utility is used downloading content from servers. This content will be in HLS or
DASH format and can be encrypted or not and live or VOD. This can result in 1000s of
files being downloaded.

Manitest(s) are first downloaded and from parsing them, a list of segment files is 
derived. These latter files are then downloaded separately. If the manifest describe 
multiple media types and bit rates, then these are all downloaded in parallel.

Individual options detailed by invoking with --help 
"""

NUM_DOWNLOADERS=3

def write_file(fn, data, prefix=''):
    """
    Write out a blob of data to a file and optionally create any
    directories.
    """
    fn = delHTTP(fn)
    print(prefix, "Write file", fn, len(data))
    mode = "wb" if type(data) is bytes else "w"

    try:
        f = open(fn, mode)

    except FileNotFoundError:
        parts = fn.rsplit('/', 1)

        if len(parts) < 2: raise

        os.makedirs(parts[0], exist_ok=True)
        f = open(fn, mode)

    f.write(data)
    f.close()

class Connect_list(object):
    """
    manage a pool of connections based upon host name.
    """
    default_host = ''
    default_type = HTTPConnection

    def __init__(self):
        self.conn_list = {}

    def do_request(self, url):
        """
        Make a request to a URL. If this is the first time for a target host,
        then open a new connection to the host.
        """
        hostname = self.default_host
        conn_type = self.default_type
        ret_fn = None

        for rdir in range(4):
            if url.startswith("http://"):
                conn_type = HTTPConnection
                hostname, url = url[7:].split('/', 1)

            elif url.startswith("https://"):
                conn_type = HTTPSConnection
                hostname, url = url[8:].split('/', 1)

            if ret_fn is None:
                ret_fn = url

            if hostname in self.conn_list:
                conn = self.conn_list[hostname]

            else:
                if ':' in hostname:
                    to_host, to_port = hostname.rsplit(':', 1)
                    conn = conn_type(to_host, int(to_port))
                else:
                    conn = conn_type(hostname)

                self.conn_list[hostname] = conn
                print("Open connection to", hostname)

            try:
                conn.request("GET", '/' + url)
                resp = conn.getresponse()
                if resp.status == 200: return ret_fn, resp

            except RemoteDisconnected:
                print("Remote end", hostname, "has disconnected session")
                del self.conn_list[hostname]
                continue
            except:
                print("Connection Failed")
                exit(1)

            if resp.status in [ 301, 302 ]:
                new_loc = resp.headers["Location"]
                resp.read()

                #print("    redirect", url, new_loc)
                url = new_loc
                continue

            if resp.status == 404:
                print("    Url not found", url)
            elif resp.status == 403:
                print("    Url forbidden", url)
            elif resp.status == 400:
                print("    Url bad", url)
            else:
                print("http error", resp.status, resp.reason, resp.headers)

            resp.read()
            return ret_fn, None

        print("Too many redirects or reties", url)
        return ret_fn, None

    def shutdown(self):
        """
        Terminate all connections
        """
        for conn in self.conn_list.values(): conn.close()
        self.conn_list = {}

    @classmethod
    def set_default(self, url):
        """
        Set the default target for file name requests.
        """
        if url.startswith("http://"):
            url = url[7:]
        elif url.startswith("https://"):
            url = url[8:]
            self.default_type = HTTPSConnection

        try:
            self.default_host, fn = url.split('/', 1)
        except:
            print("Missing directory component from url")
            return None

        return fn


class Seg_downloader(object):
    """
    Worker thread for downloading files.
    """
    threads = []
    inqueue = queue.SimpleQueue()
    shutdown = False
    thread_no = 0
    missing = []

    def __init__(self):
        self.conn = Connect_list()
        Seg_downloader.thread_no += 1
        self.no = Seg_downloader.thread_no

        self.me = threading.Thread(target=self.run, daemon=True)
        self.me.start()
        self.threads.append(self)

    def run(self):
        """
        Run the downloader. Pull next item from queue and download it
        abd write it to a file.
        """
        print("Downloader starting", self.no)

        while not self.shutdown or not self.inqueue.empty():
            try:
                url = self.inqueue.get(timeout=1)

                fn, resp = self.conn.do_request(url)

                if resp is None:
                    self.missing.append(fn)
                elif fn is not None:
                    write_file(fn, resp.read(), "%4d %2d " % ( self.inqueue.qsize(), self.no ))

            except queue.Empty:
                pass


        self.conn.shutdown()
        self.threads.remove(self)
        print("Downloader stopping", self.no)

    @classmethod
    def add(self, fn):
        """
        Add a file to be donwloaded to the queue.
        """
        self.inqueue.put(fn)

    @classmethod
    def stop_all(self):
        """
        Stop all downloader threads.
        """
        print("Stopping", len(self.threads), "Downloaders with", self.inqueue.qsize(), "outstanding")
        self.shutdown = True

        while len(self.threads) > 0: time.sleep(1)

class Man_downloader(object):
    """
    Manage polling for changes in a list of manifests and initiating a per
    type processing when a change has occured..
    """

    def __init__(self, conn_list, man_type, flist, args=None):
        self.conn_list = conn_list
        self.shutdown = False

        self.last_read = { fn: b'' for fn in flist }
        self.check = { fn: man_type(fn) for fn in flist }

        self.seg_down = True

    def run(self, max_time, poll_intv):
        """
        Perform main processing loop. This will poll the list of URLs
        looking for changes in content. When a change is detected, the
        checker class is called and it will generate the list of additional
        file names to add to the downloaders queue.
        """

        self.max_time = max_time // poll_intv * poll_intv

        signal.signal(signal.SIGINT, self.interrupt)
        signal.signal(signal.SIGHUP, self.interrupt)
        signal.signal(signal.SIGTERM, self.interrupt)

        max_idle = 20 // poll_intv
        last_change = max_idle

        while not self.shutdown:
            for url, last_read in self.last_read.items():
                if last_read is None:
                    if last_change < 0 and not self.shutdown or len(self.last_read) <= 1:
                        print("Manifests all for VOD content - shutting down")
                        self.shutdown = True

                    last_change -= 1
                    continue

                fn, resp = self.conn_list.do_request(url)
                if resp is None: continue

                cur_read = resp.read()
                #print("Checking", datetime.now().ctime())

                if cur_read == last_read:
                    if last_change < 0 and not self.shutdown:
                        print("Manifests seem to be idle - shutting down")
                        self.shutdown = True

                    last_change -= 1
                    continue

                last_change = max_idle
                self.last_read[url] = cur_read
                vod, flist = self.check[url].changed(cur_read, self)

                if self.seg_down:
                    for fn in flist: Seg_downloader.add(fn)

                if vod: 
                    self.last_read[url] = None

            if self.max_time == 0: 
                break
            time.sleep(poll_intv)
            self.max_time -= poll_intv

        for man in self.check.values(): man.terminate()

    def interrupt(self, signum, frame):
        print("Interrupt. Shutdown in progress", signum)
        self.shutdown = True


class HLS_check(object):
    """
    Handle the checking of a changed HLS manifest.
    """
    def __init__(self, fn):
        self.fn = fn.rsplit('?', 1)[0] if '?' in fn else fn
        self.seg_no = {}
        self.file_no = 0
        self.vod = None

    def changed(self, cur_read, owner):
        """
        Called when a change has been seen of the manifest contents
        """
        man = HLSManifest(self.fn, content=cur_read)

        flist = man.get_seg_list(self.seg_no, abs_paths=True)
        if self.vod is None: self.vod = man.check_vod()

        man.trim_urls()
        self.file_no += 1

        if self.file_no == 1 and self.vod:
           wr_fn="%s" % ( self.fn )
        else:
            wr_fn="%s.%s" % ( self.fn, self.file_no )

        #Write exactly as recieved
        write_file("%s.orig%s" % ( wr_fn, datetime.now().strftime("%y%m%d_%H%M%S") ), cur_read)
        #Write as recreated
        write_file(wr_fn, str(man), datetime.now().ctime())

        return self.vod, flist

    def terminate(self):
        """
        Called when shutdown invoked.
        """
        print("HLS Terminated manifest", self.fn)


class DASH_check(object):
    """
    Handle the checking of a changed DASH manifest.
    """
    def __init__(self, bands=None):
        self.bands = bands
        self.seg_no = {}
        self.file_no = 0
        self.vod = None

    def pass_thru(self, fn):
        """
        helper to allow class to be created outside of Man_downloader
        creation.
        """
        self.fn = fn.rsplit('?', 1)[0]
        return self

    def changed(self, cur_read, owner):
        """
        Called when a change has been seen of the manifest contents
        """

        man = DASHManifest(self.fn, content=cur_read)
        if self.bands: 
            man.reduce_bandwidth(self.bands)

        flist = man.get_seg_list(self.seg_no, abs_paths=True)
        if self.vod is None: 
            self.vod = man.check_vod()

        man.trim_urls()

        if man.poll_interval is None or self.vod:
            self.file_no += 1

            if self.file_no == 1 and self.vod:
                wr_fn = "%s" % ( self.fn )
            else:
                wr_fn = "%s.%s" % ( self.fn, self.file_no )

            #Writing exactly as recieved
            write_file("%s.orig%s" % (wr_fn, datetime.now().strftime("%y%m%d_%H%M%S") ), cur_read)
            #Writing as recreated
            write_file(wr_fn, str(man), datetime.now().ctime())

            return self.vod, flist

        intv = int(man.poll_interval)
        max_time = owner.max_time // intv * intv


        print("Switching to static manifest with time based segment files")

        if owner.seg_down:
            while not owner.shutdown:
                for fn in flist: Seg_downloader.add(fn)

                if max_time == 0: break

                time.sleep(man.poll_interval)
                max_time -= intv

                flist = man.get_seg_list(self.seg_no, abs_paths=True)

        write_file("%s" % ( self.fn ), str(man), datetime.now().ctime())

        owner.max_time = 0
        return True, []

    def terminate(self):
        """
        Called when shutdown invoked.
        """
        print("DASH Terminated manifest", self.fn)

def get_manifest_type(fn, args):
    """
    Set the type of the download to be used: HLS/DASH
    """
    if args.override: return args.override.lower()

    if fn.endswith(".m3u8") or ".m3u8." in fn: return "hls"

    if fn.endswith(".mpd") or ".mpd." in fn: return "dash"

    print("ERROR Unknown manifest type from ",fn)
    exit(1)

def test():
    """
    Very basic testing by harvesting from various streams which may or may not be available at some future date. 
    It is left to the operator to confirm that appropriate data has been written to the test directory created
    One way this may be done is by playing back out to aamp using simlinear.py
    """
    URLS=[
    #DASH
    { 'URL':'https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd'},
    { 'URL':'https://752d29ed521d4abe827aa6dd999dd9c5.mediatailor.ap-southeast-2.amazonaws.com/v1/dash/6c977b4f05b6d516365acf5b2c1772257652525a/l2v-pre-mid-post-brian/out/v1/fd1a1f0db1004e3fb6a038d758ad413e/24ecd621af46441296b9379d4904ce23/43240b0b39ec452b8d2a8f1607a0f54a/index.mpd'},
    #The following has content protection
    { 'URL':'https://lin013-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/SCINCOH_HD_SU_SKYUK_4019_0_6771210893185225163.mpd'},
    #HLS
    { 'URL':'https://cph-p2p-msl.akamaized.net/hls/live/2000341/test/master.m3u8'},
    { 'URL':'http://video-origin-skit-skyip.skyeucidcf.synamedialabs.com/service/1001/1001.m3u8'},
    #Test BW limiting option
    { 'URL':'https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd',
     'options':['-b','562800','-b' ,'1328400']},
    { 'URL':'https://cph-p2p-msl.akamaized.net/hls/live/2000341/test/master.m3u8',
     'options':['-b','550172','-b' ,'1650064','-b' ,'2749539']},
    #Will not harvest over NFS mount due to : in path and aamp on Ubuntu will not play it
    { 'URL':'https://vs-cmaf-pushb-ww.live.cf.md.bbci.co.uk/x=3/i=urn:bbc:pips:service:bbc_arabic_tv/pc_hd_abr_v2.mpd'},
    ]

    for idx,data in enumerate(URLS):
        test_dir="harvest_test{}".format(idx)
        url=data['URL']
        options=data.get('options',[])
        os.makedirs(test_dir,exist_ok=True)
        cmd = [sys.argv[0]] + options + ["-r",test_dir,url]
        print(cmd)
        playback = re.sub(r'(.+?//.+?/)','http://127.0.0.1:8085/' ,url)
        print("playback URL ",playback)
        print()
        result =subprocess.run(cmd,capture_output=True)
        if result.returncode != 0:
            print(result.stderr)
            print("FAILED")
    exit() #not expected to return

#Expecting at least this version. It might run with earlier versions
assert sys.version_info >= (3, 9)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Harvests content from media streams')
    parser.add_argument('-r', '--root', 
                        help="""Location to store the harvested content. Defaults to current working directory
                        """)
    parser.add_argument('-o', '--override', 
                        help="""Specifically indicate DASH or HLS content. Normally, this
                        can be inferred from the file type in the URL i.e .mpd or .m3u8""")
    parser.add_argument('-p', '--poll', type=int, 
                        help="""The number of seconds between polling for changes to the manifests.
                        If no changes are detected with the manifests for a number of polls 
                        or the manifests indicate VOD content, then the downloading will automatically stop.""",
                        default=2)
    parser.add_argument('-m', '--maxtime', type=int, 
                        help="""The number of seconds that downloading will run for. Once past this time 
                        or earlier if no manifest changes are detected, downloading of the manifests
                        will stop and downloading of the segment files will allowed to quiesce before 
                        termination. This can also be triggered by a SIGINT (CTrl+C)""",
                        default=40)
    parser.add_argument('-b', '--bandwidths', action='append', type=int, 
                        help="""Restrict processing to just the specified bit rates. If an invalid 
                        value is provided or it would remove all of one media type, then this 
                        is an error. This maybe supplied multiple times. This can be used if 
                        having ABR ability for the content is not required.""")
    parser.add_argument('-t', '--test', action='store_true',
                        help='Run some tests. Needs a dummy URL to be given E.G %(prog)s --test xxx')
    parser.add_argument('url', metavar='URL', help="URL from which to obtain content")

    args = parser.parse_args()


    if args.test:
        test()

    else:
        fn = Connect_list.set_default(args.url)
        if fn is None: 
            exit(1)

        ftype = get_manifest_type(fn, args)
        conn_list = Connect_list()

        ret_fn, resp = conn_list.do_request(fn)
        if resp is None: 
            exit(1)

        if args.root is not None: 
            os.chdir(args.root)

        write_harvest_details()

        for i in range(NUM_DOWNLOADERS): Seg_downloader()

        if ftype == "hls":
            content = resp.read()
            man = HLSManifest(fn, content=content)

            if man.is_master_manifest:
                man = HLSMainManifest(fn, content=content)
                #This is a 'top level' manifest
                print(str(man))

                if args.bandwidths and not man.reduce_bandwidth(args.bandwidths):
                    print("Bandwidths invalid", man.rept_bands())
                    exit(1)

                flist = man.get_sub_files(abs_paths=True)
                print(flist)

                if '?' in fn: fn = fn.rsplit('?', 1)[0]

                #Write exactly as recieved
                write_file("%s.orig%s" % ( fn, datetime.now().strftime("%y%m%d_%H%M%S") ), content)

                man.trim_urls()
                write_file(fn, str(man))

            else:
                flist = [ fn ]

            man_down = Man_downloader(conn_list, HLS_check, flist, args)

        elif ftype == "dash":

            content = resp.read()
            #Write exactly as recieved
            write_file("%s.orig%s" % ( fn, datetime.now().strftime("%y%m%d_%H%M%S") ), content)

            man = DASHManifest(fn, content=content)
            print(str(man))

            if args.bandwidths and not man.reduce_bandwidth(args.bandwidths):
                print("Bandwidths invalid", man.rept_bands())
                exit(1)

            check = DASH_check(args.bandwidths)

            man_down = Man_downloader(conn_list, check.pass_thru, [ fn ], args)

        else:
            print("Unrecognised URL type: try using -o option")
            exit(1)

        man_down.run(args.maxtime, args.poll)

    Seg_downloader.stop_all()

    print("Missing details:", len(Seg_downloader.missing))
    for fn in Seg_downloader.missing: print("  ", fn)

    exit(0)
