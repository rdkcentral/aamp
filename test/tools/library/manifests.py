#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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
import os
import sys
import json
from datetime import *
import time
import re
import copy
import logging
import getpass
from enum import Enum
from pathlib import Path
import urllib

def delHTTP(line):
    result = re.sub(r"^https?://", "", line)
    return result


def delHTTPhost(line):
    result = re.sub(r"^https?://.+?/", "", line)
    return result


def write_harvest_details(more_details, ftype):
    """
    Record some useful details about this harvest
    """
    time_str = datetime.utcnow().isoformat()
    
    playback = ""
    if ftype == "hls" or ftype == "dash":
        playback = "aamp/test/tools/simlinear/simlinear.py --"+ ftype +" 8085"
    
    foundreason = False
    significance = ""
    for each in sys.argv:
        if foundreason:
            significance = each
            break
        if each == "--reason":
            foundreason = True
            
    foundjira = False
    ticket = ""
    for each in sys.argv:
        if foundjira:
            ticket = each
            break
        if each == "--jira":
            foundjira = True
        
    user = getpass.getuser()
    data = {"recording_start_time": time_str, "args": sys.argv, "playback_command": playback, "significance": significance, "jira_ticket": ticket, "user": user}
    data.update(more_details)

    with open("harvest_details.json", "w") as f:
        log.debug("%s", json.dumps(data))
        f.write(json.dumps(data))


def read_harvest_details():
    d = {}
    path = os.path.abspath(os.getcwd())
    targetFile = "harvest_details.json"
    fileLocation = "/"
    while True:
        currentCheck = os.path.join(path,targetFile)
        if os.path.isfile(currentCheck):
            #Found File
            print(targetFile + " found at " + currentCheck)
            fileLocation = currentCheck
            break
        else:
            if currentCheck == targetFile:
                print(targetFile + " not found")
                break
            else:
                pathList = path.split("/")
                s = "/"
                path = s.join(pathList[:-1])
            
    if fileLocation != "/":
        with open(fileLocation) as f:
            d = json.load(f)
    return d
    

def write_transcode_details():
    """
    Record some useful details about this harvest
    """
    time_str = datetime.utcnow().isoformat()
    user = getpass.getuser()
    data = {"transcode_start_time": time_str, "args": sys.argv, "user": user}
    #data.update(more_details)

    with open("transcode_details.json", "w") as f:
        log.debug("%s", json.dumps(data))
        f.write(json.dumps(data))


def read_transcode_details():
    d = {}
    with open("transcode_details.json") as f:
        d = json.load(f)

    return d


#################################################################
class ManifestServerCommon:
    """
    Methods for reading from previously harvested manifests that
    are common to both DASH and HLS
    """

    time_of_first_manifest = 0
    time_started_serving = 0
    time_offset = 0
    serving_manifest_id = 1 # RDKAAMP-1833
    # { path_to_manifest: [(ts from mf,path_to_manifest_increment),() ... ]
    manifest_list = {}
    
    # to allow to update the offset time
    def set_offset_time(self, offset_time):
        ManifestServerCommon.time_offset = offset_time
        log.info("changing offst time from 0 to %d", offset_time)


    def return_file_index(self, path):
        """
        Returns numeric suffix from filename
        a/b/c/manifest.abc.10  returns 10
        """
        num = 0
        pattern_match = re.match(r".*\.([0-9]+)", path)
        if pattern_match:
            num = int(pattern_match.group(1))
        return num

    def get_manifest_time(self, mfile):
        """
        Read the time stamp from manifest that determines when the inremental
        manifest will be served.
        return as epoch time
        """
        time_field_re = self.get_timestamp_re(
            mfile
        )  # Different method for DASH and HLS

        try:
            with open(mfile, "r") as f:
                for line in f:
                    # print(line)
                    pattern_match = re.match(time_field_re, line)
                    if pattern_match:
                        # 19 characters gives "2023-06-15T08:16:32"
                        # but avoids fractions or Z or timezone that may occur after
                        # and we are not interested in
                        time_field = pattern_match.group(1)[:19]
                        date = time.strptime(time_field, "%Y-%m-%dT%H:%M:%S")
                        # print("time_field={} d={}".format(time_field,d))
                        return time.mktime(date)
                log.warning("WARNING did not find %s in %s", time_field_re, mfile)
                return time.mktime(time.gmtime())
        except OSError as exception:
            log.error("Exception %s $s", type(exception), exception)
            return None

    def get_list_of_manifest_timestamps(self, path):
        """
        Get a list of all the timestamps for this manifest
        cache the list in self.manifest_list
        e.g
        path something like test2/manifest_bitrate.m3u8 (or .mpd)
        directory test2/ contains
          manifest_bitrate.m3u8.1
          manifest_bitrate.m3u8.2

        returns [ (timestamp1, mf.1),(timestamp2, mf.2) ..]
        sorted by lowest file suffix first
        """
        (base, filename) = os.path.split(path)
        if base == "":
            base = "."
        # print("get_list_of_manifest_timestamps base ",base)
        # looking for all files ../some_manifest.m3u8.[n]
        files = []
        if not path in self.manifest_list:
            for file in os.listdir(base):
                re_pattern = r"{}\.[0-9]+$".format(filename)
                pattern_match = re.match(re_pattern, file)
                if pattern_match:
                    # print("Adding",file)
                    files.append(base + "/" + file)

            files_sorted = sorted(files, key=self.return_file_index)
            files_with_timestamps = []
            for file in files_sorted:
                time_stamp = self.get_manifest_time(file)
                if time_stamp:
                    tup = (time_stamp, file)
                    files_with_timestamps.append(tup)
            self.manifest_list.update({path: files_with_timestamps})
        #log.info("get_list_of_manifest_timestamps %s : %s",path,self.manifest_list[path])
        return self.manifest_list[path]

    def manifest_serve(self, path):
        """
        path is somthing like a/b/c/manifest.mpd
        Returns one of
        path if path exists
        OR
        path.n if indexed manifests do exist I.E path.1 path.2 ...
        n is dependant on timstamp in the manifest
        OR
        None if path and path.n do not exist
        """
        # print("manifest_serve:", path)
        file_timestamps = self.get_list_of_manifest_timestamps(path)
        if os.path.exists(path) and file_timestamps == []:
            # With request for non-incremental manifest then we
            # assume this is top level manifest so reset timer
            # used for serving incremental
            self.time_started_serving = 0
            log.info("Playing main Manifest")
            return path
        #file_timestamps = self.get_list_of_manifest_timestamps(path)

        # RDKAAMP-1833
        pub_times = [f[0] for f in file_timestamps]
        if len(set(pub_times)) <= 1:
            self.serving_manifest_id
            for timestamp, manifest_file in file_timestamps:
                if manifest_file.endswith(f".{self.serving_manifest_id}"):
                    break
            self.serving_manifest_id += 1 
            return manifest_file
        if len(file_timestamps) == 0:
            # No manifests - need return 404
            log.info("Not found index %s", path)
            return None

        if len(file_timestamps) == 1:
            # Occurs for VOD
            first_ts, file_to_serve = file_timestamps[0]
            return file_to_serve

        first_ts, file_to_serve = file_timestamps[0]
        if self.time_started_serving == 0:
            # First manifest to be served, initialise timestamps
            self.time_of_first_manifest = first_ts
            self.time_started_serving = time.time()

        # look for the timestamp and hence corresponding manifest
        # that we need to serve at this time. This will be the highest
        # number timestamp that does not exceed
        # time_manifest as calculated below
        time_elapsed = time.time() - self.time_started_serving
        time_manifest = self.time_of_first_manifest + time_elapsed + self.time_offset

        for timestamp, manifest_file in file_timestamps:
            if timestamp > time_manifest:
                break
            file_to_serve = manifest_file
        return file_to_serve

    def get_timestamp_re(self, path):
        """
        Returns re pattern to extract timestamp from DASH/HLS manifest
        """
        if ".mpd" in path:
            re_pattern = r'.*publishTime="(.*?)"'
        elif ".m3u8" in path:
            re_pattern = (
                r"#(?:SIMLINEAR-PDT-OVERRIDE|EXT-X-PROGRAM-DATE-TIME):([0-9T:\-]+)"
            )
        else:
            log.error("Cannot determine manifest type from %s", path)
            exit(1)
        return re_pattern


class Manifest:
    """
    Represent a general manifest for media stream.
    """

    def __init__(self, path):
        self.orig_path = path
        self.content = []
        self.rule_list = []
        self.encrypted = False

        self.orig_date = None
        self.first_date = None
        self.last_date = None
        self.window = timedelta(0, 15)
        self.poll_interval = None

        self.first_seg = 0
        self.cnt_seg = 0
        self.first_disc = 0
        self.bands = {}

        self.reset_genr()

    def reset_genr(self):
        """
        Reset the silding window for the generate() call
        """
        self.cur_seg = None
        self.cur_disc = None

        self.last_idx = -1
        self.last_date = None

    date_form = "%Y-%m-%dT%H:%M:%S.%f"
    date_form_lst = [
        date_form,
        "%Y-%m-%dT%H:%M:%S",
        "%Y-%m-%dT%H:%M:%S.%f%z",
        "%Y-%m-%dT%H:%M:%S%z",
        "%Y-%m-%dT%H:%M:%S.%f%Z",
        "%Y-%m-%dT%H:%M:%S%Z",
    ]
    date_formtz = date_form_lst[2]

    # Handle python prior to 3.7
    try:
        datetime.strptime("2022-04-18T21:28:44Z", date_form_lst[3])
    except:
        date_form_lst += ["%Y-%m-%dT%H:%M:%S.%fZ", "%Y-%m-%dT%H:%M:%SZ"]

    def date_parse(self, value):
        """
        Helper routine to parse a date/time string
        """
        for df in self.date_form_lst:
            try:
                return datetime.strptime(value, df).replace(tzinfo=None)
            except:
                pass

        else:
            raise Exception("Invalid date format:" + value)

    def fmt_date(self, conv):
        """
        Helper to format a date/time from a datetime instance.
        """
        return conv.strftime(self.date_form)[:-3] + "Z"

    def adj_char_date(self, value, adj):
        """
        Helper to parse a date, add an adjustment and then format the result.
        """
        for df in self.date_form_lst:
            try:
                dt = datetime.strptime(value, df) + adj
                return dt.strftime(df)
            except:
                pass

        else:
            raise Exception("Invalid date format:" + value)

    def abs_path(self, fn, base=None):
        """
        Convert paths relative to this manifest to 'absolute path names in the server.
        base = https://a.com/a/b/c  fn = d/e/f  returns https://a.com/a/b/c/d/e/f
        """
        if not base:
            base = self.orig_path

        new_path = urllib.parse.urljoin(base, fn)
        return new_path

    def rept_bands(self):
        res = {}

        for band, mime_type in self.bands.items():
            res.setdefault(mime_type[0], []).append(band)

        return res


class SegmentList:
    """
    Class for accumulating list of segment files and then
    returning in various forms
    """

    def __init__(self, segments_already_added = []):
        self.segment_detail_list = []

        # For each segment group an incrementing counter
        self.play_order = {}

        # List of segments we have already added to segment_detail_list
        # to avoid duplicates
        self.segment_url_list = segments_already_added

        # attributes for each segment_group
        self.attributes = {}

    def new_file(self, segment_group, encrypted, attrs=None):
        """
        Called before a new group of segments is added via add_init()/add_file()
        """
        self.key = segment_group
        attrs.encrypted = encrypted

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

            if hasattr(attrs, "pts") and self.attributes.get(segment_group, 0) != 0: # RDKAAMP-1834
                delattr(attrs, "pts")
                attrs.pts = self.attributes[segment_group].pts


        self.attributes[segment_group] = copy.copy(attrs)

    def add_init(self, url, segment_group,segment_filename=None):
        """
        Call for when a stream initialisation file is to be added to the list.
        """
        self.add_file(url, 0, segment_group,segment_filename=segment_filename)

    def add_file(self, segment_url, segment_duration, segment_group, segment_t = -1, segment_d = -1, segment_filename=None):
        """
        Call for when a segment needs to be added.

        segment_url:      path to segment it's url or filename
        segment_duration: The duration of this segment (seconds)
        segment_group :   The bandwidth or representation id of segment
        segment_t:        The pts t value read from the manifest
        segment_d:        The d value read from the manifest
        segment_filename  Local path to file
        """

        if segment_url in self.segment_url_list:
            log.debug(f"Ignore duplicate {segment_duration} {segment_url}")
            # Allow duplication of header segment. Since for some streams, header segments have same name for multiple periods
            if  segment_duration > 0 :
                return
            # Do not add multiple header segments in the same segment group
            for detail in self.segment_detail_list:
                if detail["segment_url"] == segment_url and detail["profile"] == segment_group:
                    return

        self.segment_url_list.append(segment_url)

        play_order = self.play_order.get(segment_group, 0)
        play_order += 1
        self.play_order.update({segment_group: play_order})

        if segment_filename is None:
            log.error(f"No filename given for {segment_url}")
            exit(1)

        segment_details = {
            "segment_url": segment_url,
            "profile": segment_group,
            "play_order": play_order,
            "duration": segment_duration,
            "segment_t": segment_t,
            "segment_d": segment_d,
            "segment_filename": segment_filename,
        }

        self.segment_detail_list.append(segment_details)
        log.debug(f"{segment_details}")

    def get_segment_groups(self):
        return self.play_order.keys()

    def get_attributes(self, segment_group):
        return self.attributes[segment_group]

    def get_segments(self, segment_group):
        """
        return ordered list of segments belonging to specific segment_group
        """
        list = []
        for segment_details in self.segment_detail_list:
            if segment_details["profile"] == segment_group:
                list.append(segment_details)

        # print("get_segments",list)
        return sorted(list, key=lambda x: x.get("play_order"))

    def dump_info(self,group=None):
        """
        Output data that class is holding.
        group parameter to limit to one group
        """
        groups = self.get_segment_groups()
        if group:
            groups = [group]

        for segment_group in groups:
            attrs = self.get_attributes(segment_group)
            log.debug("attrs %s",attrs.__str__())
            seg_list = self.get_segments(segment_group)
            log.debug("%s total_segments=%d listing first 5",segment_group, len(seg_list))
            for ent in seg_list[:5]:
                log.debug("%s",ent)

    def __iter__(self):
        """
        Return an iterator over all the segments

        The list is returned after sorting on 2 parameters:
        1) sorted by the play_order value so that segments played first are returned first
        2) sorted alphabetically so that for example '480p_003.m4s' is always before '720p_003.m4s'
        """

        self.rtn_list = sorted(
            self.segment_detail_list,
            key=lambda x: (x.get("play_order"), x.get("segment_url")),
        )

        return iter(self.rtn_list)


log = logging.getLogger("root")

if __name__ == "__main__":
    base_dir = os.path.dirname(sys.argv[0])
    os.chdir(base_dir + "/..")

    time.sleep(1)
    print("no tests at the moment")
