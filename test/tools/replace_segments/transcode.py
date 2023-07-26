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
import sys
import os
import argparse
import logging
from urllib.parse import urlparse

from library.manifests import (
    write_harvest_details,
    delHTTP,
    SegmentList,
    ManifestServerCommon,
    read_harvest_details,
)
from library.hls_manifests import HLSManifest, HLSMainManifest
from library.dash_manifests import DASHManifest
from library.manifests import write_transcode_details
from mp4_tools import *


HELP = """
This utility is used for inspecting and modifying download media streams. Although it
is specifically aimed at transcoding any encrypted contents that may have been
downloaded, it can transcode any of the contents.

The downloaded contents are composed of a set of logical files that have been broken
down into segments. These logical files may represent different bit rates,
different media types (audio/video), adverts etc. and any combinations thereof.
Each may be handled independently.
"""


class fromman_fileList(transcode_flist):
    """
    Derived class to build the list of output files and their durations from
    the list returned from get_seg_list() calls.
    """

    def __init__(self, slist, out_dir=None):
        super().__init__(out_dir)

        for fileName, duration in slist:
            duration = round(duration, 3)

            if duration < 0.5:
                self.init_file = fileName
                continue

            self.flist.append(fileName)
            self.dur_list.append(duration)
            self.tot_duration += duration

        self.tot_duration = round(self.tot_duration, 3)
        log.info("Total duration of %d", self.tot_duration)


def get_dash_segments_from_multiple_manifests(manifest_path):
    """
    Reads dash manifest or set of increments and returns
    cumulative segment list
    """

    file_timestamps = []

    manifest_server = ManifestServerCommon()

    segment_list = SegmentList()
    if os.path.exists(manifest_path):
        # single manifest not incremental
        file_timestamps = [(0, manifest_path)]
    else:
        file_timestamps = manifest_server.get_list_of_manifest_timestamps(manifest_path)

    # iterate through all manifests
    for man_time, man_path in file_timestamps:
        man = DASHManifest(man_path)
        man.get_seg_list(seg_list=segment_list, abs_paths=True)

        man.remove_encryption_tags() #Remove encrypted tags from xml
        with open(man_path, "w") as f_stream:
            f_stream.write(str(man))


    return segment_list


def get_hls_segments_from_multiple_manifests(manifest_path):
    """
    Reads HLS manifest , set of incremental sub manifests
    and returns cumulative segment list
    """

    log.debug("HLS manifest %s", manifestFilename)

    file_timestamps = []

    manifest_server = ManifestServerCommon()
    segment_list = SegmentList()
    man = HLSMainManifest(manifestFilename)

    sub_manifest_list = man.get_sub_files(abs_paths=True)
    for manifest_path in sub_manifest_list:
        log.info("Sub manifest %s", manifest_path)
        file_timestamps = manifest_server.get_list_of_manifest_timestamps(manifest_path)
        for man_time, man_path in file_timestamps:
            man1 = HLSManifest(man_path)
            man1.get_seg_list(seg_list=segment_list, abs_paths=True)

    return segment_list


def get_manifest_path():
    """
    From information saved at the start of harvest establishes the path 
    to the initial manifest file. The file may not exist but incremental ones
    will exist
    E.G
    Initial URL:
    http://server.com/a/b/c/manifest.mpd

    returned by this function:
    a/b/c/manifest.mpd

    actual files that shoudl exist after harvest:
    a/b/c/manifest.mpd.1
    a/b/c/manifest.mpd.2
    """
    harvest_details = read_harvest_details()

    if "url" in harvest_details:
        url = harvest_details["url"]
    else:
        # Old method, no so good
        url = harvest_details["args"][-1]

    o = urlparse(url)

    # Loose the leading '/'
    return o.path[1:]


#####################################################
def check_have_ffprobe():
    """
    Check that ffmpeg utils are installed. Give clear error
    now rather than an obsecure one later
    """
    rtn = subprocess.run(["ffprobe", "-version"])
    if rtn.returncode != 0:
        log.error("Cannot run ffprobe. Is ffmpeg installed?")
        exit(1)


#######################################################
logging.basicConfig(
    format="%(funcName)-15s:%(lineno)04d %(message)s",
    stream=sys.stdout,
)
log = logging.getLogger("root")
log.setLevel(logging.INFO)

if __name__ == "__main__":
    if sys.argv[1] in ["-z", "--doc"]:
        print(HELP)
        exit(0)

    parser = argparse.ArgumentParser(
        description="Takes a harvested stream and replaces encrypted segments with clear."
    )
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help="Process unencrypted files.  By default, only encrypted content will be processed.",
    )
    parser.add_argument(
        "-r",
        "--root",
        help="Root directory for the files; if not supplied, the current directory is assumed",
    )
    parser.add_argument(
        "--no_segments",
        action="store_true",
        help="""Assume no segments are present and create them from manifest 
        data. The default is to create new segments using the duration information 
        from existing segments.""",
    )
    parser.add_argument(
        "-t",
        "--transcode",
        metavar="donor_video",
        required=True,
        help="Specify the video to use as a donor video for transcode.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Display more detail during execution",
    )

    args = parser.parse_args()

    log.debug("args: %s", args)

    if args.root is not None:
        os.chdir(args.root)

    if args.verbose:
        log.setLevel(logging.DEBUG)
        
    write_transcode_details()

    manifestFilename = get_manifest_path()

    if not os.path.exists(args.transcode):
        log.error("Cannot find %s", args.transcode)
        exit(1)

    if manifestFilename.endswith(".m3u8") or ".m3u8." in manifestFilename:
        segment_list = get_hls_segments_from_multiple_manifests(manifestFilename)

    elif manifestFilename.endswith(".mpd") or ".mpd." in manifestFilename:
        segment_list = get_dash_segments_from_multiple_manifests(manifestFilename)

    else:
        log.error("Unrecognised file type")
        exit(1)

    audio_cnt = 0
    video_cnt = 0
    other_cnt = 0

    if args.verbose:
        segment_list.dump_info()

    set_workdir()

    for segment_group in segment_list.get_segment_groups():

        attrs = segment_list.get_attributes(segment_group)
        encrypted = attrs.encrypted
        if args.all is False and encrypted is False:
            # Skip transcode for clear stream
            log.info("NOT Processing segment_group %s", segment_group)
            continue

        log.info("Processing segment_group %s", segment_group)

        if args.no_segments:
            # Use the segment durations specified in the manifest
            segment_and_duration = []
            for segment_detail in segment_list.get_segments(segment_group):
                tup = (segment_detail["segment_name"], segment_detail["duration"])
                segment_and_duration.append(tup)
            log.info("segment_and_duration %S", segment_and_duration)
            proc_list = fromman_fileList(segment_and_duration, "./")
        else:
            # Derive the segment durations from the original segment length
            check_have_ffprobe()
            segment_names = []
            for segment_detail in segment_list.get_segments(segment_group):
                segment_names.append(segment_detail["segment_name"])
            # log.debug("segment_names %s",segment_names)
            proc_list = mpeg_flist(segment_names, "./")

        base_dir = os.path.dirname(os.path.realpath(__file__))
        log.info("attrs %s",attrs.__str__())
        do_transcode(
            base_dir,
            proc_list,
            args.transcode,
            attrs,
        )

    exit(0)
