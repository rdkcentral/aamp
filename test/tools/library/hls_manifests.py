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

"""
To test any changes use
aamp/test/tools/library/test_toolchain.py
######################################################################
"""

import os
import sys
import re
import logging
from datetime import datetime, timedelta, timezone
import time
from pathlib import Path
from library.manifests import delHTTPhost, Manifest, SegmentList
from library.attriblist import AttribList
import urllib
import uuid
from library.filesys_utils import url_to_filename

# Keep a list of the segments we have already requested for download so when 
# a changed manifest is parsed we do not request segments requested previously
segments_already_added = []

class HLSManifest(Manifest):
    """
    Represent the contents of a HLS video or audio manifest and
    provides manipulation.
    """

    def __init__(
        self, path, default_start=None, content=None, attrs=None, post_process=True
    ):
        super().__init__(path)

        self.disc_crypt = set()
        if attrs is None:
            self.attrs = AttribList("")
        else:
            self.attrs = attrs
        self.is_master_manifest = True


        if content is None:
            with open(path, "rb") as f:
                content = f.read()

        self.load_contents(post_process, content, lambda x: x.decode())

        if post_process:
            self.orig_date = self.calc_range(default_start)

    def load_contents(self, post_process, content, adj_line=lambda x: x):
        """
        Load manifest from a file or other stream device
        """
        last_path = None
        disc_no = 0

        for line in content.splitlines():
            parts = adj_line(line).strip("\n").split(":", 1)

            if len(parts) < 2:
                value = parts[0]
                self.content.append(["", value])
                if not value.startswith("#"):
                    last_path = value
                continue

            key, value = parts

            if key == "#EXT-X-PROGRAM-DATE-TIME" or key == "#SIMLINEAR-PDT-OVERRIDE":
                dt_time = self.date_parse(value)
                if self.orig_date is None:
                    self.orig_date = dt_time

                self.content.append([key, dt_time if post_process else value])

            elif key == "#EXT-X-MEDIA-SEQUENCE":
                self.first_seg = int(value)
                self.content.append([key, value])

            elif key == "#EXT-X-DISCONTINUITY-SEQUENCE":
                self.first_disc = disc_no = int(value)
                self.content.append([key, value])

            elif key == "#EXTINF":
                self.content.append([key, value])


            elif key in ["http", "https"]:
                self.content.append(["", key + ":" + value])

            elif key in ["#EXT-X-KEY", "#xEXT-X-KEYx"] and post_process:
                self.encrypted = True
                self.disc_crypt.add(disc_no)
                self.content.append([key, value])

            else:
                self.content.append([key, value])


        if self.orig_date == None:
            self.orig_date = now = datetime.now()
            self.content.insert(1, ["#SIMLINEAR-PDT-OVERRIDE", now])

    def extinf_to_num(self, value):
        return float(value.split(",")[0])

    def calc_range(self, default_start):
        """
        Calculate the date/time range for the manifest.
        """
        start = None
        end = datetime.now() if default_start is None else default_start
        self.cnt_seg = 0

        for key, value in self.content:
            if key == "#EXT-X-PROGRAM-DATE-TIME" or key == "#SIMLINEAR-PDT-OVERRIDE":
                end = value
                if not start:
                    start = end

            elif key == "#EXTINF":
                if not start:
                    start = end
                end = end + timedelta(0, self.extinf_to_num(value))
                self.cnt_seg += 1
        log.info("start %s", start)
        if not start:
            return None

        self.first_date = start
        self.last_date = end

        return start + timedelta(0)

    def write(self, path=None):
        """
        Write the manfest to a file.
        """
        if path is None:
            path = self.orig_path

        with open(path, "w") as f:
            f.write(str(self))

    def validate(self, dirn="", rept_miss=""):
        """
        Check which media files referenced by the manifest exist or not i.e
        have been downloaded.
        """
        ok_cnt = 0
        missing_lst = []

        if dirn != "" and not dirn.endswith("/"):
            dirn += "/"

        for key, value in self.content:
            if key == "#EXT-X-MAP":
                url = AttribList(value).URI
            elif not key and not value.startswith("#"):
                url = value
            else:
                continue

            fn = dirn + self.abs_path(url)

            if Path(fn).is_file():
                ok_cnt += 1

            else:
                # if len(missing_lst) < 10: print(fn)
                missing_lst.append(url)
                if rept_miss and rept_miss in fn or rept_miss == "*":
                    log.info("fn %s", fn)

        log.info(
            "For %s:  OK files: %d  Missing files: %d",
            self.orig_path,
            ok_cnt,
            len(missing_lst),
        )
        return ok_cnt, missing_lst

    def __str__(self):
        """
        Convert the whole of the manifest to a string.
        """

        res = ""

        for key, value in self.content:
            if type(value) is datetime:
                res += key + ":" + self.fmt_date(value) + "\n"
            elif type(value) is float:
                res += key + ":" + str(value) + ",\n"
            else:
                res += (key + ":" + str(value) if key else str(value)) + "\n"
            """
            For simlinear.py playback we add a timestamp #SIMLINEAR-PDT-OVERRIDE so that
            simlinear.py knows when to serve up the incremental manifest again. I.E the timestamp records the time difference
            between each incremental manifest
            """
            if value == "#EXTM3U":
                res += (
                    "#SIMLINEAR-PDT-OVERRIDE:"
                    + self.fmt_date(datetime.now(timezone.utc))
                    + "\n"
                )

        return res

    def check_vod(self):
        """
        Indicate as to whether this is VOD as apposed to live content.
        """
        key, value = self.content[-1]
        return key == "" and value == "#EXT-X-ENDLIST"

    def set_encrypt(self, state=False):
        """
        Hide or expose any encryption keys.
        """
        cnt = 0

        for line in self.content:
            key, value = line
            if key not in ["#EXT-X-KEY", "#xEXT-X-KEYx"]:
                continue

            line[0] = "#EXT-X-KEY" if state else "#xEXT-X-KEYx"
            cnt += 1

        return cnt

    def remove_increment(self, path):
        """
        For a path like a/b/c/manifest.m3u8.1
        returns
        a/b/c/manifest.m3u8
        """

        return re.sub(r"\.[0-9]+$", "", path)

    def get_seg_list(self, from_seg=None, abs_paths=False, seg_list=None):
        """
        Returns the files names associated with segments.
        """
        if from_seg is None:
            from_seg = {}

        """
        Keep one segment maintained over processing of incremental manifests
        This allows to keep track of segments already downloaded so we do not
        download multiple times
        """
        seg_list = SegmentList(segments_already_added=segments_already_added)

        cur_disc = self.first_disc
        disc_no = cur_disc - 1
        duration = None


        segment_group = self.remove_increment(self.orig_path)
        seg_list.new_file(segment_group, cur_disc in self.disc_crypt, self.attrs)

        # Process each of the lines in the file and optionally skipping previously handled
        # files
        for line in self.content:
            key, value = line
            if key == "#EXTINF":
                duration = self.extinf_to_num(value)

            elif key == "" and value == "#EXT-X-DISCONTINUITY":
                cur_disc += 1

                segment_group = self.remove_increment(self.orig_path)

                log.info("new_file")
                seg_list.new_file(segment_group, cur_disc in self.disc_crypt, self.attrs)


            elif key == "#EXT-X-MAP" and disc_no != cur_disc:
                url = AttribList(value).URI
                segment_group = self.remove_increment(self.orig_path)
                seg_list.new_file(
                    segment_group,
                    cur_disc in self.disc_crypt,
                    self.attrs,
                )

                url1 = self.abs_path(url) if abs_paths else url
                seg_filename = url_to_filename(url1)
                seg_list.add_init(url1, segment_group, segment_filename=seg_filename)
                disc_no = cur_disc


            elif not key and value and not value.startswith("#"):
                seg_path_parsed = urllib.parse.urlparse(value)
                url = value
                if seg_path_parsed.netloc and not seg_path_parsed.query:
                    """
                    url starts with domain name and no query, assuming sensible path on another server
                    E.G https://live-content-cf.xumo.com/3516/content/XM09DMUR8UJRUF/24117079/4_124.ts
                    becomes
                    live-content-cf.xumo.com/3516/content/XM09DMUR8UJRUF/24117079/4_124.ts
                    """
                    seg_filename = seg_path_parsed.netloc + seg_path_parsed.path
                    log.debug(f"seg_filename {value} {seg_filename}")
                    # File written to harvest root so update manifest to fetch from server root
                    line[1] = "/" + seg_filename
                elif not seg_path_parsed.netloc and not seg_path_parsed.query :
                    # simple path on server hosting manifests
                    url = self.abs_path(value)
                    seg_filename = urllib.parse.urlparse(url).path[1:]
                    log.debug(f"seg_filename {value} {seg_filename}")
                elif seg_path_parsed.netloc and  seg_path_parsed.query:
                    """
                    E.G
                    https://hls-beacons-us.xumo.com/hlsstream/v1/beacon?url=https%3A%2F%2Flive-content-cf.xumo.com%2F3516%2Fcontent%2FXM09DMUR8UJRUF%2F24117079%2F4_127.ts&aid=XM09DMUR8UJRUF&eventType=ASSET&cid=88889153&did=5817fb989eb54e87&playId=1731000592032&eventSubType=playInterval&pid=351
                    becomes
                    hls-beacons-us.xumo.com/fb7ff1dc77924c589832c0929b962484
                    """
                    seg_filename = seg_path_parsed.netloc + "/" + uuid.uuid4().hex
                    log.debug(f"seg_filename {value} {seg_filename}")
                    # File written to harvest root so update manifest to fetch from server root
                    line[1] = "/" + seg_filename
                else:
                    log.error(f"Unsupported {value}")
                    log.error(f"Unsupported {seg_path_parsed}")
                    exit(1)
                segment_group = self.remove_increment(self.orig_path)
                seg_list.add_file(url, duration, segment_group, segment_filename=seg_filename)

        return seg_list

    def trim_urls(self):
        """
        Remove all of the hostname parts of any URLs defined in the manifest.
        """
        for ent in self.content:
            key, value = ent

            if key == "#EXT-X-MAP":
                attrlst = AttribList(value)
                attrlst.URI = delHTTPhost(attrlst.URI)
                ent[1] = str(attrlst)


class HLSMainManifest(Manifest):
    """
    Represents a top level HLS manifest that describes the audio and video
    manifests.
    """

    def __init__(self, path, content=None):
        super().__init__(path)

        self.sub_list = {}

        if content is None:
            with open(path, "rb") as f:
                content = f.read()
        self.load_contents(content, lambda x: x.decode())

    def load_contents(self, content, adj_line=lambda x: x):
        """
        Load manifest from a file or other stream device
        """
        attrs = None
        band = None

        for line in content.splitlines():
            line = adj_line(line)

            if line.startswith("#EXT-X-SESSION-KEY:"):
                continue

            line = line.strip()

            if line.startswith("#EXT-X-MEDIA:"):
                attrs = AttribList(line, start=":")

                if hasattr(attrs, "URI") and hasattr(attrs, "TYPE"):
                    mime_type = attrs.TYPE.lower()
                    self.add_sub_man(mime_type, attrs.URI, attrs)

            elif line.startswith("#EXT-X-STREAM-INF:"):
                attrs = AttribList(line, start=":")

                if hasattr(attrs, "BANDWIDTH"):
                    band = int(attrs.BANDWIDTH)
                    self.bands.setdefault(band, ["video"]).append(line)

            elif not line.startswith("#") and line != "" and band is not None:
                self.add_sub_man("video", line, attrs)
                self.bands[band].append(line)
                band = None

            self.content.append(line)

    def write(self, path=None):
        """
        Write the manfest to a file.
        """
        if path is None:
            path = self.orig_path

        with open(path, "w") as f:
            f.write(str(self))

    def add_sub_man(self, mime_type, uri, attrs):
        """
        Index a reference to a sub-manfest based upon nedia type.
        """
        attrs.TYPE = mime_type
        self.sub_list.setdefault(mime_type, {})[uri] = attrs

    def reduce_bandwidth(self, band_list):
        """
        Reduce down the number of bandwidths supported by the top level manifest.
        """
        if type(band_list) in [str, int]:
            band_list = [band_list]

        band_check = [int(band) for band in band_list if int(band) in self.bands]

        if len(band_check) < len(band_list) or band_list == []:
            return False

        del_lst = []

        for band, lines in self.bands.items():
            if band in band_check:
                continue

            self.content.remove(lines[1])

            if len(lines) > 2:
                self.content.remove(lines[2])
                del self.sub_list["video"][lines[2]]

            del_lst.append(band)

        for band in del_lst:
            del self.bands[band]

        return True

    def get_sub_files(self, mime_list=None, abs_paths=False):
        """
        Return the list of audio and video URLs
        """
        flist = []
        if mime_list is not None and type(mime_list) is str:
            mime_list = [mime_list]

        log.info("mime_list %s", mime_list)
        log.info("sub_list %s", self.sub_list)

        # format of sub_list = {'video': ['url1', 'url2'] }

        for mime_type, slist in self.sub_list.items():
            if mime_list is not None and mime_type not in mime_list:
                continue
            log.info("slist %s", slist)
            flist += [self.abs_path(fn) for fn in slist] if abs_paths else list(slist)

        return flist

    def __str__(self):
        """
        Return this instance as a string.
        """
        res = ""

        for line in self.content:
            res += line + "\n"

        return res

    def get_seg_list(self, from_seg=dict(), abs_paths=False, seg_list=None):
        """
        Returns the files names associated with segments.
        """
        if from_seg is None:
            from_seg = {}
        if seg_list is None:
            seg_list = seg_list_base()

        for mime_type, slist in self.sub_list.items():
            for manfn, attrs in slist.items():
                if abs_paths:
                    manfn = self.abs_path(manfn)

                get_seg_manlist(
                    manfn,
                    lambda fn: HLSManifest(fn, attrs=attrs),
                    from_seg=from_seg,
                    seg_list=seg_list,
                    abs_paths=abs_paths,
                )

        return seg_list

    def trim_urls(self):
        """
        Remove all of the hostname parts of any URLs defined in the manifest.
        """
        self.sub_list = {}
        self.bands = {}
        attrs = None
        band = None
        idx = 0

        for line in self.content:
            if line.startswith("#EXT-X-MEDIA:"):
                attrs = AttribList(line, start=":")

                if hasattr(attrs, "URI") and hasattr(attrs, "TYPE"):
                    attrs.URI = delHTTPhost(attrs.URI)
                    self.content[idx] = line = "#EXT-X-MEDIA:" + str(attrs)

                    mime_type = attrs.TYPE.lower()
                    self.add_sub_man(mime_type, attrs.URI, attrs)

            elif line.startswith("#EXT-X-STREAM-INF:"):
                attrs = AttribList(line, start=":")

                if hasattr(attrs, "BANDWIDTH"):
                    band = int(attrs.BANDWIDTH)
                    self.bands.setdefault(band, ["video"]).append(line)

            elif not line.startswith("#") and line != "":
                self.content[idx] = line = delHTTPhost(line)

                self.add_sub_man("video", line, attrs)
                self.bands[band].append(line)
                band = None

            idx += 1


log = logging.getLogger("root")

if __name__ == "__main__":
    base_dir = sys.argv[0]
    base_dir = base_dir[: base_dir.rfind("/")]
    os.chdir(base_dir)

    print("Starting test run")

    TOP_SUB_FILES = [
        "test/discontinuity_test_audio_1_stereo_128000.m3u8",
        "test/discontinuity_test_video_180_250000.m3u8",
        "test/discontinuity_test_video_270_400000.m3u8",
        "test/discontinuity_test_video_360_800000.m3u8",
        "test/discontinuity_test_video_540_1200000.m3u8",
        "test/discontinuity_test_video_720_2400000.m3u8",
        "test/discontinuity_test_video_1080_4800000.m3u8",
    ]

    SEG_LIST = [
        "video/270_400000/hls/segment_1.ts",
        "video/270_400000/hls/segment_2.ts",
        "video/270_400000/hls/segment_3.ts",
        "video/270_400000/hls/segment_4.ts",
        "video/270_400000/hls/segment_5.ts",
        "video/270_400000/hls/segment_6.ts",
        "video/270_400000/hls/segment_7.ts",
        "video/270_400000/hls/segment_8.ts",
        "video/270_400000/hls/segment_9.ts",
        "video/270_400000/hls/segment_10.ts",
    ]

    # Parse some sample manifests and confirm we get the right number of segments
    TEST_MANIFESTS1 = [
        {
            "top_manifest": True,
            "manifest": "test/top_manifest.m3u8",
            "expected_list": TOP_SUB_FILES,
        },
        {
            "top_manifest": False,
            "manifest": "test/discontinuity_test_video_270_400000.m3u8.1",
            "expected_list": SEG_LIST,
        },
    ]

    for t in TEST_MANIFESTS1:
        print(60 * "=")
        print(t["manifest"])

        if t["top_manifest"]:
            man = HLSMainManifest(t["manifest"])
            flist = man.get_sub_files(abs_paths=True)
        else:
            man = HLSManifest(t["manifest"])
            flist = []
            for segment_details in man.get_seg_list(abs_paths=True):
                flist.append(segment_details['segment_name'])
        if flist != t["expected_list"]:
            print("FAILED unexpected list {}", flist)
            exit(-1)
        else:
            print("PASSED")
