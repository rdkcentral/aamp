#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

import sys
import os
import subprocess
import argparse
import logging
from urllib.parse import urlparse
import re
import struct

from library.manifests import (
    SegmentList,
    ManifestServerCommon,
    read_harvest_details,
)

from library.dash_manifests import DASHManifest
from library.manifests import write_transcode_details

def get_dash_segments_from_multiple_manifests(manifest_path):
    """
    Reads dash manifest or set of increments and returns
    cumulative segment list
    """
    global args
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

def check_ffmpeg_version():
    result = subprocess.run("ffmpeg -version | grep version", shell=True,check=True,capture_output=True)
    line = result.stdout.decode('utf-8')
    log.debug(line)
    m = re.search(r' version ([\d\.]+)',line )
    if m:
        ver = float(m.group(1))
        log.info(f"ffmpeg Version {ver}")
        if ver >= 7.1:
            return

    log.error("Incorrect version or cannot establish ffmpeg version. Need >7.1")
    exit(1)
#####################################################

def do_transcode(base_dir, segment_detail_list, attrs=None):
    """
    Given an class trancode_flist, use this as a template for generating replacement
    media files from a skeleton media file base upon bit rates etc. Then resegment
    the output files to match the originals.
    """

    log.debug("")
    generate_segment = os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "generate-segment")
    codec = None
    # Codec from manifest to codec used by ffmpeg
    if re.match(r'hvc',attrs.codecs):
        codec = 'hevc'
    elif re.match(r'mp4a',attrs.codecs):
        codec = 'aac'
    elif re.match(r'ec-3',attrs.codecs):
        codec = 'eac3'
    elif re.match(r'ac-3',attrs.codecs):
        codec = 'ac3'
    elif re.search(r'ttml',attrs.codecs):
        codec = 'ttml'
    else:
        codec=attrs.codecs

    # Init seg is first in list and does not need processing
    init_segment=segment_detail_list.pop(0)
    if init_segment['duration'] != 0:
        log.error(f"Expecting zero duration for init {init_segment}")
        return
    init_segment_path=os.path.join(base_dir, init_segment['segment_name'])

    # codec errors from ffmpeg cannot write file out
    head, tail = os.path.split(init_segment_path)
    if tail and head:
        os.makedirs(head, exist_ok=True)

    for segment_number, segment_detail in enumerate(segment_detail_list):

        segment_path = os.path.join(base_dir, segment_detail['segment_name'])
        """
        For a segment filename like
        ...ckId-104-tc-0-time-719214900493.mp4
        then take the last 9 digits and use that as the sequence number to
        display on the video frame
        """
        m = re.search(r'(\d{1,6})\.m',segment_detail['segment_name'])
        if m:
           segment_number=int(m.group(1))

        # Generate iframes track to have a segment with single iframe
        iframe_rate = int(attrs.timescale)/int(segment_detail['segment_d']) # I.E 1/duration

        if attrs.contentType == "video":
            cmd = [ "./generate-video-segment.sh",
                   attrs.width,
                   attrs.height,
                   getattr(attrs,"frameRate", f"{iframe_rate}"), # iframe track has no frameRate attribute
                   codec,
                   str(segment_detail['segment_t']),
                   str(segment_detail['segment_d']),
                   attrs.timescale,
                   str(segment_number),
                   segment_path,
                   init_segment_path,
                   "testpat.jpg"
                   ]
        elif attrs.contentType == "audio":
            cmd = [ "./generate-audio-segment.sh",
                   attrs.audioSamplingRate,
                   codec,
                   str(segment_detail['segment_t']),
                   str(segment_detail['segment_d']),
                   attrs.timescale,
                   str(segment_number),
                   segment_path,
                   init_segment_path,
                   "silence.wav"
                   ]
        else:
            log.error(f"Unsupported {attrs.contentType}")
            break

        log.debug(cmd)
        rtn = subprocess.run(cmd, shell=False, cwd=generate_segment)
        if rtn.returncode != 0:
            log.error("Failed")
            exit(-1)

    log.info("do_transcode finished")

logging.basicConfig(
    format="%(funcName)-15s:%(lineno)04d %(message)s",
    stream=sys.stdout,
)
log = logging.getLogger("root")
log.setLevel(logging.INFO)

if __name__ == "__main__":

    check_ffmpeg_version()

    parser = argparse.ArgumentParser(
        description="""Takes a harvested stream and replaces encrypted segments with clear. 
Typically invoked in a directory where harvest_details.json resides"""
    )
    parser.add_argument(
        "-a",
        "--all",
        action="store_true",
        help="Process unencrypted files.  By default, only encrypted content will be processed.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Display more detail during execution",
    )
    parser.add_argument(
        "-m",
        "--manifest",
        help="Specify manifest instead of using harvest details file",
    )
    parser.add_argument(
        "--start_at",
        help="""Only transcode segments from periods with start="xx" or greater. To avoid transcode of 
past segments in cloud TSB.
E.G --start_at PT480253H21M58.641S """,
    )
    args = parser.parse_args()

    log.debug("args: %s", args)


    if args.verbose:
        log.setLevel(logging.DEBUG)

    write_transcode_details()

    if args.manifest:
        manifestFilename = args.manifest
    else:
        manifestFilename = get_manifest_path()


    #######################################################


    if manifestFilename.endswith(".mpd") or ".mpd." in manifestFilename:
        segment_list = get_dash_segments_from_multiple_manifests(manifestFilename)

    else:
        log.error("Unrecognised file type")
        exit(1)

    for segment_group in segment_list.get_segment_groups():

        attrs = segment_list.get_attributes(segment_group)
        encrypted = attrs.encrypted
        if args.all is False and encrypted is False:
            # Skip transcode for clear stream
            log.info("NOT Processing segment_group %s", segment_group)
            continue

        """
        For dash manifest with cloud buffer then we want to limit segment selection
        from harvest time forward rather than all segments in the manifest which will
        cover the previous 30mins (for a 30min buffer)
        """
        if args.start_at:
            period_start = DASHManifest.PT_parse_secs(attrs.period_start)
            user_start = DASHManifest.PT_parse_secs(args.start_at)
            if user_start >period_start:
                log.info(f"Skip {segment_group} because before start time {args.start_at}")
                continue


        log.info("Processing segment_group %s", segment_group)
        segment_list.dump_info(segment_group)
        segment_detail_list = segment_list.get_segments(segment_group)

        do_transcode(
            os.getcwd(),
            segment_detail_list,
            attrs,
        )

    exit(0)
