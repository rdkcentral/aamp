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
import sys, os

base_dir = os.path.dirname(os.path.realpath(__file__))

import time
from datetime import *
import re
import argparse
import threading
import subprocess
import queue
import signal
from pathlib import Path
from library.manifests import write_harvest_details, delHTTP, get_seg_manlist
from library.hls_manifests import HLSManifest, HLSMainManifest
from library.dash_manifests import DASHManifest

from mp4_tools import *
from library.filesys_utils import *
from log import log

HELP = """
This utility is used for inspecting and modifying download media streams. Although it
is specifically aimed at transcoding any encrypted contents that may have been
downloaded, it can transcode any of the contents.

The downloaded contents are composed of a set of logical files that have been broken
down into segments. These logical files may represent different bit rates,
different media types (audio/video), adverts etc. and any combinations thereof.
Each may be handled independently.
"""


class segment_tree():
    """
    Class for structuring the response from the get_seg_list() calls. This gathers the
    additional attributes from the HLS/DASH manifest classes and encapsulates them
    for use during the transcoding.
    """

    def __init__(self, do_all=False):
        self.slist = []
        self.do_all = do_all
        self.cur_list = None

        self.key = None
        self.encypted = None
        self.attrs = None

    def new_file(self, key, encrypted, attrs=None):
        """
        Called for when a new logical segmented file is encountered. The different
        attributes need to coalesed into a common set.
        """
        self.key = key
        self.encrypted = encrypted

        if args.verbose:
            print("segment_tree.newfile()")
            print("attrs: ", attrs)

        if attrs is not None:
            if not hasattr(attrs, "TYPE") and hasattr(attrs, "mimeType"):
                attrs.TYPE = attrs.mimeType
                if "/" in attrs.TYPE:
                    attrs.TYPE = attrs.TYPE.split("/")[0]

            if hasattr(attrs, "FRAME-RATE"):
                attrs.FPS = round(float(getattr(attrs, "FRAME-RATE")))
            elif hasattr(attrs, "frameRate"):
                attrs.FPS = attrs.frameRate.split("/")[0]

            if hasattr(attrs, "bandwidth"):
                attrs.BANDWIDTH = attrs.bandwidth

            if hasattr(attrs, "height"):
                attrs.RESOLUTION = attrs.width + "x" + attrs.height

        self.attrs = attrs

    def add_init(self,fn):
        """
        Call for when a stream initialisation file is to be atted to the list.
        """
        self.add_file(fn,0)

    def play_order_reset(self):
        pass

    def add_file(self, fn, duration):
        """
        Called for when a segment file needs to be added. The first time following
        a new file call, the detail about the stream will be added to the list.
        """
        if args.verbose:
            print("segment_tree.add_file() entry")

        if self.key is None:
            pass

        elif self.do_all or self.encrypted:
            self.cur_list = []
            self.slist.append((self.key, self.encrypted, self.attrs, self.cur_list))
            self.key = None

        else:
            self.cur_list = None
            self.key = None

        if self.cur_list is not None:
            self.cur_list.append((fn, duration))

        if args.verbose:
            print("segment_tree.newfile() - ")
            print("cur_list:", self.cur_list)
            log("slist: ", self.slist)

    def __iter__(self):
        """
        Return an iterator over the resultant list.
        """
        return iter(self.slist)

class fromman_fileList(transcode_flist):
    """
    Derived class to build the list of output files and their durations from
    the list returned from get_seg_list() calls.
    """

    def __init__(self, slist, out_dir=None):
        super().__init__(out_dir)

        for fileName, duration in slist:
            log("*" * 20, fileName, "*" * 20)
            duration = round(duration, 3)

            if duration < 0.5:
                self.init_file = fileName
                continue

            self.flist.append(fileName)
            self.dur_list.append(duration)
            self.tot_duration += duration

        self.tot_duration = round(self.tot_duration, 3)
        log(
            "Total duration of",
            len(self.flist),
            "files:",
            self.tot_duration,
            "list:",
            self.dur_list,
        )


def test():
    url = "https://752d29ed521d4abe827aa6dd999dd9c5.mediatailor.ap-southeast-2.amazonaws.com/v1/dash/6c977b4f05b6d516365acf5b2c1772257652525a/l2v-pre-mid-post-brian/out/v1/fd1a1f0db1004e3fb6a038d758ad413e/24ecd621af46441296b9379d4904ce23/43240b0b39ec452b8d2a8f1607a0f54a/index.mpd"

    # fileName = Connect_list.set_default(url)

    # conn_list = Connect_list()
    # fileName, resp = conn_list.do_request(fileName)
    # if resp is None: exit(1)

    # man = DASHManifest(fileName, stream=resp)
    # f = open("fred.xml", "w")
    # log(str(man), file=f)

    # fileList = man.get_seg_list(abs_paths=True)

    # for fileName in fileList:
    #     if "asset-2500init.mp4" in fileName: break

    # #log(subprocess.run(["curl -Lv '%s' > fred" % fileName ], shell=True, check=True))

    # fileName, resp = conn_list.do_request(fileName)
    # log(len(resp.read()))

    exit(0)


def add_root(root, fileName):
    """
    Add root diractory from download to file name.
    """
    return fileName if fileName.startswith(root) else root + fileName


def filter_bandwidths(man, args):
    """
    Filter down manifest contents to just those for a specific bandwidth"
    """
    if args.bandwidths and not man.reduce_bandwidth(args.bandwidths):
        log("Bandwidths invalid", man.rept_bands())
        exit(1)


if __name__ == "__main__":
    if sys.argv[1] in ["-z", "--doc"]:
        print(HELP)
        exit(0)

    parser = argparse.ArgumentParser(
        description="Startup server for servicing media streams"
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="This lists the logical files described by the manifests and lists the associated segment files.",
    )
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help="Process unencrypted files.  By default, only encrypted content will be processed.",
    )
    parser.add_argument(
        "-f", "--filter", help="A text string to match against the segment file names."
    )
    parser.add_argument(
        "-r",
        "--root",
        help="Root directory for the files; if not supplied, the current directory is assumed",
    )
    parser.add_argument(
        "-m",
        "--mandur",
        action="store_true",
        help="Use segment durations as specified in manifest rather than derived from the content of output files",
    )
    parser.add_argument(
        "-b",
        "--bandwidths",
        action="append",
        type=int,
        help="Restrict processing to just the specified bit rates. If an invalid value is provided or it would remove all of one media type, then this is an error. This may be supplied multiple times.",
    )
    parser.add_argument(
        "-t",
        "--transcode",
        help="Perform transcoding of the selected segment files using the file or URL specified as a skeleton. This process will be repeated for each logical file that has been passed through the above criteria.",
    )
    parser.add_argument(
        "-R", "--restore", action="store_true", help="Restore file back from backup"
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Display more detail during execution",
    )
    parser.add_argument("url", metavar="URL", help="URL from which to obtain content")
    args = parser.parse_args()

    if args.verbose:
        log("args: ", args)

    root = "./" if args.root is None else args.root

    if not root.endswith("/"):
        root += "/"

    manifestFilename = args.url

    if not manifestFilename.startswith("/"):
        manifestFilename = add_root(root, manifestFilename)

    file_filter = None

    if args.filter:
        try:
            file_filter = re.compile(
                ("" if args.filter.startswith("^") else ".*?") + args.filter
            )

        except:
            log("Problem with file name filter regular expression")
            exit(1)

    seg_list = segment_tree(args.all)

    if manifestFilename.endswith(".m3u8") or ".m3u8." in manifestFilename:
        if args.verbose:
            log("HLS manifest ", manifestFilename)

        if Path(manifestFilename).is_file():
            man = HLSManifest(manifestFilename, post_process=True)

            if args.verbose:
                log("Manifest: ")
                log(man)

            if not man.has_segments:
                log("Loaded top level manifest %s" % manifestFilename)
                man = HLSMainManifest(manifestFilename)
                filter_bandwidths(man, args)

            elif args.bandwidths:
                log("Cannot bandwidth filter on low level HLS manifest")
                exit(1)

            man.get_seg_list(abs_paths=True, seg_list=seg_list)

        else:
            if args.bandwidths:
                log("Assumption is low level HLS manifests so no bandwidth filtering")
                exit(1)

            get_seg_manlist(
                manifestFilename, HLSManifest, abs_paths=True, seg_list=seg_list
            )

    elif manifestFilename.endswith(".mpd") or ".mpd." in manifestFilename:
        if args.verbose:
            log("DASH manifest")
        get_seg_manlist(
            manifestFilename,
            DASHManifest,
            abs_paths=True,
            seg_list=seg_list,
            oper=lambda m: filter_bandwidths(m, args),
        )

    else:
        log("Unrecognised file type")
        exit(1)

    audio_cnt = 0
    video_cnt = 0
    other_cnt = 0

    if args.verbose:
        log("Segment list:")

    for key, encrypted, attrs, slist in seg_list:
        if args.verbose:
            log("Key: ", key, " encrypted:", encrypted, " attrs:", attrs)

    for key, encrypted, attrs, slist in seg_list:
        if not hasattr(attrs, "TYPE"):
            other_cnt += len(slist)
        elif attrs.TYPE == "video":
            video_cnt += len(slist)
        elif attrs.TYPE == "audio":
            audio_cnt += len(slist)
        else:
            other_cnt += len(slist)

    if args.verbose:
        log("end segment list")

    log(
        "#",
        20 * "=",
        "Scanning through manifests has revealed",
        video_cnt,
        "video segments,",
        audio_cnt,
        "audio segments and",
        other_cnt,
        "other type of segments",
        20 * "=",
    )

    if args.list:
        for key, encrypted, attrs, slist in seg_list:
            if args.verbose:
                log("key ", key)
            if file_filter is not None and not file_filter.match(key):
                continue

            log("#" + key + (" (Encrypted)" if encrypted else " clear"), str(attrs))

            for fileName, duration in slist:
                log(add_root(root, fileName))

        # exit(0)

    set_workdir()
    orig_dir = os.getcwd()

    for key, encrypted, attrs, slist in seg_list:
        if file_filter is not None and not file_filter.match(key):
            continue
        if args.verbose:
            log("# Starting processing segments for", key)
        fileName_iter = iter(slist)
        hl_fileName, duration = next(fileName_iter)

        init_fileName = os.path.basename(hl_fileName)
        dn = os.path.dirname(hl_fileName)
        missing = 0

        if dn != "":
            if manifestFilename.startswith("/"):
                clip = len(dn) + 1
            else:
                dn = dn[1:]
                clip = len(dn) + 2

            log(dn, hl_fileName)
            os.chdir(add_root(root, dn))

        else:
            clip = 0

        log("#      in ", dn)

        if duration <= 0:
            fileList = [init_fileName]
        else:
            fileList = []
            init_fileName = ""
            fileName_iter = iter(slist)

        for fileName, duration in fileName_iter:
            fileName = fileName[clip:]

            try:
                stat = os.stat(fileName)
                fileList.append(fileName)

            except FileNotFoundError:
                log(10 * "-", fileName)
                missing += 1

        if args.restore:
            for fileName in fileList:
                if Path("./orig_bak/" + fileName).is_file():
                    copy_file("./orig_bak/" + fileName, fileName, False)

        else:
            if args.mandur:
                # Use the segment durations specified in the manifest
                proc_list = fromman_fileList(
                    [(fileName[clip:], duration) for fileName, duration in slist], "./"
                )
            else:
                # Derive the segment durations from the original segment length
                proc_list = mpeg_flist(fileList, "./")

            if args.transcode is None:
                if proc_list.tot_duration >= 0.5:
                    log(
                        "#     Located files",
                        len(fileList),
                        "Split points:",
                        proc_list.split_points(),
                    )

            else:
                if args.verbose:
                    log("Calling do_transcode")

                do_transcode(
                    base_dir,
                    proc_list,
                    args.transcode
                    if args.transcode.startswith("/")
                    else orig_dir + "/" + args.transcode,
                    attrs,
                )

        os.chdir(orig_dir)
        log("#     Missing files", missing)

    exit(0)
