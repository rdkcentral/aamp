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


import sys
import os
from datetime import datetime
import time
import re
import argparse
import threading
import queue
import signal
import logging
import requests
import urllib

from library.hls_manifests import HLSManifest, HLSMainManifest
from library.dash_manifests import DASHManifest
from library.manifests import write_harvest_details


HELP = """
This utility is used downloading content from servers. This content will be in HLS or
DASH format and can be encrypted or not and live or VOD. This can result in 1000s of
files being downloaded.

Manifest(s) are first downloaded and from parsing them, a list of segment files is 
derived. These latter files are then downloaded separately. If the manifest describe 
multiple media types and bit rates, then these are all downloaded in parallel.

Individual options detailed by invoking with --help 
"""

NUM_DOWNLOADERS = 6


def add_timestamp(filename):
    """
    Add date time stamp to filename
    """
    return f"{filename}.orig{datetime.utcnow().strftime('%y%m%d_%H%M%S')}"


def write_file(url, data):
    """
    Write out a blob of data to a file and optionally create any
    directories.
    """

    file_name = url_to_filename(url)
    mode = "wb" if isinstance(data, bytes) else "w"

    log.info("%s size=%d ts=%d", file_name, len(data), time.time())

    head, tail = os.path.split(file_name)

    if tail and head:
        os.makedirs(head, exist_ok=True)

    with open(file_name, mode) as f_stream:
        f_stream.write(data)


def url_to_filename(url):
    """
    Convert url into a relative path that can be used for writing data to
    E.G
    http://video-origin-skit-skyip.skyeucidcf.synamedialabs.com/service/1001/1001.m3u8
    becomes
    service/1001/1001.m3u8
    """
    o = urllib.parse.urlparse(url)
    # strip leading /
    path = o.path[1:]
    return path


class SegmentDownloader:
    """
    Worker thread for downloading files.
    """

    threads = []
    inqueue = queue.SimpleQueue()
    shutdown_when_empty = False
    shutdown_immediatly = False
    thread_no = 0
    missing = []

    def __init__(self):
        self.requests_session = requests.Session()
        SegmentDownloader.thread_no += 1
        self.thread_number = SegmentDownloader.thread_no

        self.thread_me = threading.Thread(target=self.run, daemon=True)
        self.thread_me.start()
        self.threads.append(self)

    def run(self):
        """
        Run the downloader. Pull next item from queue and download it
        abd write it to a file.
        """
        log.info("Downloader starting %d", self.thread_number)

        while not (self.shutdown_when_empty and self.inqueue.empty()) and not self.shutdown_immediatly:
            try:
                url = self.inqueue.get_nowait()

                resp = self.requests_session.get(url)

                if resp.ok:
                    write_file(url, resp.content)
                else:
                    log.error("status_code=%d %s", resp.status_code, url)
                    self.missing.append(url)

            except queue.Empty:
                pass

        self.threads.remove(self)
        log.info("Downloader stopping %s", self.thread_number)

    @classmethod
    def add(cls, filename):
        """
        Add a file to be downloaded to the queue.
        """
        cls.inqueue.put(filename)

    @classmethod
    def stop_all(cls,shutdown_immediatly=False):
        """
        Stop all downloader threads.
        """
        log.info(
            "Stopping threads=%d qsize=%s", len(cls.threads), cls.inqueue.qsize()
        )
        cls.shutdown_when_empty = True
        cls.shutdown_immediatly = shutdown_immediatly

        while len(cls.threads) > 0:
            time.sleep(1)


class ManifestDownloader:
    """
    Manage polling for changes in a list of manifests and initiating a per
    type processing when a change has occured..
    """

    def __init__(self, requests_session, manifest_checker, url_list, args):
        self.requests_session = requests_session
        self.shutdown = False

        self.last_read = {filename: b"" for filename in url_list}
        self.check = {filename: manifest_checker(filename) for filename in url_list}
        self.max_time = None
        self.args = args
        self.do_download_segments = not args.no_segments

        self.seg_requested_duration_totals = {}

    def segment_list_process(self, segment_list, is_vod):
        """
        Takes list of segments and adds them to the segment downloader until
        the total segment time for each profile/bandwidth meets that required
        for harvest

        return True if segment(s) were added to downloader
        """
        did_add_segment = False
        for segment_details in segment_list:
            profile = segment_details["profile"]
            total = self.seg_requested_duration_totals.get(profile, 0)
            if total < self.max_time or is_vod is False:
                self.seg_requested_duration_totals[profile] = (
                    total + segment_details["duration"]
                )
                segment_name = segment_details["segment_name"]
                # print("Adding {}",segment_details)
                SegmentDownloader.add(segment_name)
                did_add_segment = True
            else:
                pass
        return did_add_segment

    def run(self, max_time, poll_intv):
        """
        Perform main processing loop. This will poll the list of URLs
        looking for changes in content. When a change is detected, the
        checker class is called and it will generate the list of additional
        file names to add to the downloaders queue.
        """

        start_time = time.time()
        self.max_time = max_time

        signal.signal(signal.SIGINT, self.interrupt)
        signal.signal(signal.SIGHUP, self.interrupt)
        signal.signal(signal.SIGTERM, self.interrupt)

        max_idle = 20 // poll_intv
        last_change = max_idle
        last_loop_time = time.time()

        while not self.shutdown:
            for url, last_read in self.last_read.items():
                if last_read is None:
                    if (
                        last_change < 0
                        and not self.shutdown
                        or len(self.last_read) <= 1
                    ):
                        log.info("Manifests all for VOD content - shutting down")
                        self.shutdown = True

                    last_change -= 1
                    continue

                resp = self.requests_session.get(url)

                if resp.status_code != 200:
                    log.error("status_code=%d %s", resp.status_code, url)
                    continue

                cur_read = resp.content

                if cur_read == last_read:
                    if last_change < 0 and not self.shutdown:
                        log.info("Manifests seem to be idle - shutting down")
                        self.shutdown = True

                    last_change -= 1
                    continue

                last_change = max_idle
                self.last_read[url] = cur_read
                vod, segment_detail_list = self.check[url].changed(cur_read, self)

                if self.do_download_segments:
                    self.segment_list_process(segment_detail_list,vod)

                if vod:
                    self.last_read[url] = None

            if time.time() - start_time > self.max_time:
                log.info("SHUTDOWN Polling for longer than %d", self.max_time)
                self.shutdown = True

            loop_elapsed = time.time() - last_loop_time
            loop_delay = poll_intv - loop_elapsed
            log.debug(f"Sleep for {loop_delay} loop_elapsed {loop_elapsed}")
            if loop_delay > 0:
                time.sleep(loop_delay)
            last_loop_time = time.time()

        for man in self.check.values():
            man.terminate()

    def interrupt(self, signum, frame):
        """
        Handle ctrl-C or other action
        """
        log.warning("Interrupt. Shutdown in progress")
        SegmentDownloader.stop_all(shutdown_immediatly=True)
        self.shutdown = True


class HLSChecker:
    """
    Handle the checking of a changed HLS manifest.
    """

    def __init__(self, filename):
        self.filename = filename.rsplit("?", 1)[0]
        self.seg_no = {}
        self.file_no = 0
        self.vod = None

    def changed(self, cur_read, owner):
        """
        Called when a change has been seen of the manifest contents
        """
        man = HLSManifest(self.filename, content=cur_read)

        segment_detail_list = man.get_seg_list(self.seg_no, abs_paths=True)
        if self.vod is None:
            self.vod = man.check_vod()

        man.trim_urls()
        self.file_no += 1

        if self.file_no == 1 and self.vod:
            wr_fn = self.filename
        else:
            wr_fn = f"{self.filename}.{self.file_no}"

        # Write exactly as recieved
        write_file(add_timestamp(wr_fn), cur_read)
        # Write as recreated
        write_file(wr_fn, str(man))

        return self.vod, segment_detail_list

    def terminate(self):
        """
        Called when shutdown invoked.
        """
        log.info("HLS Terminated manifest %s", self.filename)


class DASHChecker:
    """
    Handle the checking of a changed DASH manifest.
    """

    def __init__(self, args):
        self.bands = args.bandwidths
        self.seg_no = {}
        self.file_no = 0
        self.filename = None
        self.vod = None
        self.args = args

    def pass_thru(self, filename):
        """
        helper to allow class to be created outside of ManifestDownloader
        creation.
        """
        self.filename = filename.rsplit("?", 1)[0]
        return self

    def changed(self, cur_read, owner):
        """
        Called when a change has been seen of the manifest contents
        """

        man = DASHManifest(self.filename, content=cur_read)
        if self.bands:
            man.reduce_bandwidth(self.bands)

        segment_detail_list = man.get_seg_list(self.seg_no, abs_paths=True)

        if self.vod is None:
            self.vod = man.check_vod()

        man.trim_urls()

        if man.poll_interval is None or self.vod:
            self.file_no += 1

            if self.file_no == 1 and self.vod:
                wr_fn = self.filename
            else:
                wr_fn = f"{self.filename}.{self.file_no}"

            # Writing exactly as recieved
            write_file(add_timestamp(wr_fn), cur_read)
            # Writing as recreated
            write_file(wr_fn, str(man))

            return self.vod, segment_detail_list

        intv = int(man.poll_interval)
        max_time = owner.max_time // intv * intv

        log.info("Switching to static manifest with time based segment files")

        if owner.do_download_segments:
            while not owner.shutdown:
                owner.segment_list_process(segment_detail_list,self.vod)

                if max_time == 0:
                    break

                time.sleep(man.poll_interval)
                max_time -= intv

                segment_detail_list = man.get_seg_list(self.seg_no, abs_paths=True)

        write_file(self.filename, str(man))

        owner.max_time = 0
        return True, []

    def terminate(self):
        """
        Called when shutdown invoked.
        """
        log.info("DASH Terminated manifest %s", self.filename)


def get_manifest_type(filename):
    """
    Set the type of the download to be used: HLS/DASH
    """
    if args.override:
        return args.override.lower()

    if filename.endswith(".m3u8") or ".m3u8." in filename:
        return "hls"

    if filename.endswith(".mpd") or ".mpd." in filename:
        return "dash"

    log.error("ERROR Unknown manifest type from %s", filename)
    sys.exit(1)


# Expecting at least this version. It might run with earlier versions
assert sys.version_info >= (3, 9)

logging.basicConfig(
    format="%(asctime)s %(funcName)-15s:%(lineno)04d %(message)s",
    stream=sys.stdout,
)

log = logging.getLogger('root')
log.setLevel(logging.INFO)
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Harvests content from media streams")
    parser.add_argument(
        "-r",
        "--root",
        help="""Location to store the harvested content. Defaults to current working directory
                        """,
    )
    parser.add_argument(
        "-o",
        "--override",
        help="""Specifically indicate DASH or HLS content. Normally, this
                        can be inferred from the file type in the URL i.e .mpd or .m3u8""",
    )
    parser.add_argument(
        "-p",
        "--poll",
        type=int,
        help="""The number of seconds between polling for changes to the manifests.
                        If no changes are detected with the manifests for a number of polls 
                        or the manifests indicate VOD content, then the downloading will automatically stop.
                        default=2s""",
        default=2,
    )
    parser.add_argument(
        "-m",
        "--maxtime",
        type=int,
        help="""For VOD asset then harvest will download this duration of segments. 
                        For a live asset then harvest will download all segments 
                        referenced by manifest and then poll for further segments 
                        for this duration.
                        default=40s""",
        default=40,
    )
    parser.add_argument(
        "-b",
        "--bandwidths",
        action="append",
        type=int,
        help="""Restrict processing to just the specified bit rates. If an invalid
                        value is provided or it would remove all of one media type, then this 
                        is an error. This maybe supplied multiple times. This can be used if 
                        having ABR ability for the content is not required.""",
    )
    parser.add_argument(
        "--reason",
        action="append",
        type=str,
        help="""For adding specified significance to command call. Reason for harvest""",
    )
    parser.add_argument(
        "--jira",
        action="append",
        type=str,
        help="""For adding related Jira Ticket. In format RDKAAMP-XXXX""",
    )

    parser.add_argument(
        "--no_segments",
        action="store_true",
        help="""Skip download of segments as they will be generated by transcode.""",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Display more detail during execution",
    )
    parser.add_argument("url", metavar="URL", help="URL from which to obtain content")

    args = parser.parse_args()

    if args.root is not None:
        os.chdir(args.root)

    if args.verbose:
        log.setLevel(logging.DEBUG)

    url = args.url
    filename_part = url_to_filename(url)
    if filename_part is None or len(filename_part) == 0:
        log.error("ERROR no filename from %s", url)
        sys.exit(1)

    ftype = get_manifest_type(filename_part)
    requests_session = requests.Session()

    response = requests_session.get(url)
    if response is None:
        log.error("ERROR no response from %s", url)
        sys.exit(1)

    if not response.ok:
        log.error("status_code=%d %s", response.status_code, url)
        sys.exit(1)

    write_harvest_details({'url':args.url}, ftype)

    """
    Only need segment downloader if we are to download segments
    It was also noticed that with multiple SegmentDownloader()
    not doing anything then the manifest fetch was taking 30+seconds.
    """
    if not args.no_segments:
        for i in range(NUM_DOWNLOADERS):
            SegmentDownloader()

    if ftype == "hls":
        content = response.content

        man = HLSManifest(filename_part, content=content)

        if man.is_master_manifest:
            # Write exactly as recieved
            write_file(add_timestamp(url), content)

            man = HLSMainManifest(url, content=content)
            # This is a 'top level' manifest
            print(str(man))

            if args.bandwidths and not man.reduce_bandwidth(args.bandwidths):
                log.error("Bandwidths invalid %s", man.rept_bands())
                sys.exit(1)

            flist = man.get_sub_files(abs_paths=True)
            log.info("url=%s flist=%s", url, flist)
            url_list = flist

            man.trim_urls()
            write_file(url, str(man))

        else:
            url_list = [url]

        log.info("url_list=%s", url_list)
        man_down = ManifestDownloader(requests_session, HLSChecker, url_list)

    elif ftype == "dash":
        content = response.content
        # Write exactly as recieved
        write_file(add_timestamp(url), content)

        man = DASHManifest(filename_part, content=content)
        print(str(man))

        if args.bandwidths and not man.reduce_bandwidth(args.bandwidths):
            log.error("Bandwidths invalid %s", man.rept_bands())
            sys.exit(1)

        check = DASHChecker(args)

        man_down = ManifestDownloader(requests_session, check.pass_thru, [url], args)

    else:
        log.info("Unrecognised URL type: try using -o option")
        sys.exit(1)

    man_down.run(args.maxtime, args.poll)

    if not args.no_segments:
        SegmentDownloader.stop_all()

        log.info("Missing details %d", len(SegmentDownloader.missing))
        with open("missing_segments.txt", "a") as f:
            f.write("Missing details " + str(len(set(SegmentDownloader.missing))) + "\n\n")
            for filename in set(SegmentDownloader.missing):
                log.info("%s", filename)
                f.write(filename + "\n\n")

    sys.exit(0)
