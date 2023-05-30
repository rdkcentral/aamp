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
import os
import sys
import json
from datetime import datetime, timedelta, timezone
import time
from pathlib import Path
from library.manifests import delHTTPhost, Manifest, seg_list_base, get_seg_manlist
from library.attriblist import AttribList


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
        self.attrs = attrs
        self.is_master_manifest = True
        self.has_segments = False

        if content is None:
            with open(path, "rb") as f:
                content = f.read()

        self.load_contents(post_process, content, lambda x: x.decode())

        if post_process:
            self.orig_date = self.calc_range(default_start)
            # if rebase and self.orig_date is not None: self.adj_dates(rebase)

    def load_contents(self, post_process, content, adj_line=lambda x: x):
        """
        Load manifest from a file or other stream device
        """
        seg_no = 0
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
                seg_no += 1

                self.content.append([key, value])

            # elif key == '#!':
            #    self.add_rule(value, self.first_seg + seg_no, last_path)
            #    self.content.append([ key, value ])

            elif key in ["http", "https"]:
                self.content.append(["", key + ":" + value])

            elif key in ["#EXT-X-KEY", "#xEXT-X-KEYx"] and post_process:
                self.encrypted = True
                self.disc_crypt.add(disc_no)
                self.content.append([key, value])

            else:
                self.content.append([key, value])

        self.has_segments = seg_no > 0

        if self.orig_date == None and seg_no > 0:
            self.orig_date = now = datetime.now()
            self.content.insert(1, ["#SIMLINEAR-PDT-OVERRIDE", now])

    def extinf_to_num(self, value):
        return float(value.split(",", 1)[0] if value.endswith(",") else value)

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
        print("start", start)
        if not start:
            return None

        self.first_date = start
        self.last_date = end

        return start + timedelta(0)

    def adj_dates(self, targ):
        """
        Adjust the all of the date/time entries in the manifest so that
        they start from the suppled point.
        """
        self.last_idx = -1
        adj = targ - self.first_date

        for line in self.content:
            if (
                line[0] == "#EXT-X-PROGRAM-DATE-TIME"
                or line[0] == "#SIMLINEAR-PDT-OVERRIDE"
            ):
                line[1] = line[1] + adj
            elif line[0] == "#EXT-X-DATERANGE":
                line[1] = self.adj_list_date(line[1], ["START-DATE", "END-DATE"], adj)

        self.first_date = targ
        self.last_date += adj

    def adj_list_date(self, src, names, adj):
        """
        Helper routine to adjust a date/time entry within a line
        containing al list of name=value entries.
        """
        namelst = src if isinstance(src, AttribList) else AttribList(src)

        for name in names:
            dt = getattr(namelst, name, None)
            if dt:
                setattr(namelst, name, self.adj_char_date(dt, adj))

        return str(namelst)

    def do_concatenate(self, src, space=0.0):
        """
        Concatenate another manifest onto this manifest.
        """
        if not isinstance(src, HLSManifest):
            raise ValueError("Invalid type (type(src)} passed to do_concatenate()")

        src.adj_dates(self.last_date + timedelta(space))
        passed_hddr = False
        do_copy = timedelta(0)

        key, value = self.content[-1]
        end_list = key == "" and value == "#EXT-X-ENDLIST"
        if end_list:
            self.content.pop(-1)

        seg_no = self.first_seg + self.cnt_seg
        last_path = None

        for key, value in src.content:
            if key == "#EXT-X-PROGRAM-DATE-TIME":
                passed_hddr = True
                self.content.append([key, value + do_copy])

            elif key == "#EXTINF":
                passed_hddr = True
                seg_no += 1
                self.content.append([key, value])

            elif passed_hddr:
                self.content.append([key, value])
                # if key == "#!": self.add_rule(value, seg_no, last_path)
                if key == "" and not value.startswith("#"):
                    last_path = value

        self.last_date = src.last_date + do_copy
        self.cnt_seg += src.cnt_seg

        if end_list:
            self.content.append(["", "#EXT-X-ENDLIST"])

        return self.last_date

    def do_merge(self, src, space=0.0):
        """
        Merge another manfest in. This assumes that the other manifest is time-shifted
        such that we only really want to extend our manifest.
        """
        if not isinstance(src, HLSManifest):
            raise ValueError("Invalid type (type(src)} passed to do_merge()")

        print("pat ", self.first_date, self.orig_date, file=sys.stderr)
        diff = self.first_date - self.orig_date
        src_diff = src.first_date - src.orig_date
        src.adj_dates(src.first_date - src_diff + diff + timedelta(space))

        if src.last_date <= self.last_date:
            return self.last_date

        found = False
        do_copy = timedelta(0)
        end_seg_no = self.first_seg + self.cnt_seg
        src_seg_no = src.first_seg
        last_path = None

        for key, value in src.content:
            if key == "#EXT-X-PROGRAM-DATE-TIME" or key == "#SIMLINEAR-PDT-OVERRIDE":
                found = src_seg_no >= end_seg_no
                # src_seg_no += 1
                if found:
                    value = value + do_copy

            elif key == "#EXTINF":
                found = src_seg_no >= end_seg_no
                src_seg_no += 1

            if not found:
                continue

            if key == "#!":
                self.add_rule(value, src_seg_no, last_path)
            elif key == "" and not value.startswith("#"):
                last_path = value

            self.content.append([key, value])

        self.cnt_seg = src_seg_no - self.first_seg
        self.last_date = src.last_date + do_copy
        return self.last_date

    def concat_discontinuity(self, src, space=0.0):
        """
        Concatenate another manifest as a discontinuity.
        """
        dur = src.last_date - src.first_date
        dur = dur.seconds + dur.microseconds / 1000000.0
        start = self.first_date + timedelta(0, space)

        self.content.append(["", "#EXT-X-DISCONTINUITY"])
        # self.content.append([ "#EXT-X-DATERANGE", 'ID="0x30-139-1665502597",START-DATE="' + start.strftime(self.date_formtz) + '",PLANNED-DURATION=' + str(cur) ])

        return self.do_concatenate(src, space)

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
                    print("     ", fn)

        print(
            "For %s:  OK files: %d  Missing files: %d"
            % (self.orig_path, ok_cnt, len(missing_lst))
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

    def get_seg_list(self, from_seg=None, abs_paths=False, seg_list=None):
        """
        Returns the files names associated with segments.
        """
        print("get_seg_list")
        if from_seg is None:
            from_seg = {}
        if seg_list == None:
            seg_list = seg_list_base()

        seg_no = from_seg.get(self.orig_path, -1)
        cur_seg = self.first_seg
        cur_disc = self.first_disc
        disc_no = cur_disc - 1
        duration = None

        if seg_no < 0:
            print("seg_no < 0")
            seg_list.new_file(
                self.orig_path + "_" + str(cur_disc),
                cur_disc in self.disc_crypt,
                self.attrs,
            )

        # Process each of the lines in the file and optionally skipping previously handled
        # files
        for key, value in self.content:
            if key == "#EXTINF":
                print("add seg")
                duration = self.extinf_to_num(value)
                cur_seg += 1

            elif key == "" and value == "#EXT-X-DISCONTINUITY":
                cur_disc += 1

                if cur_seg > seg_no:
                    print("new_file")
                    seg_list.new_file(
                        self.orig_path, cur_disc in self.disc_crypt, self.attrs
                    )

            elif cur_seg <= seg_no:
                continue

            elif key == "#EXT-X-MAP" and disc_no != cur_disc:
                url = AttribList(value).URI
                seg_list.new_file(
                    self.orig_path + "_" + str(cur_disc),
                    cur_disc in self.disc_crypt,
                    self.attrs,
                )

                seg_list.add_init(self.abs_path(url) if abs_paths else url)
                disc_no = cur_disc

            elif cur_seg <= seg_no:
                continue

            elif not key and value and not value.startswith("#"):
                seg_list.add_file(
                    self.abs_path(value) if abs_paths else value, duration
                )

        from_seg[self.orig_path] = cur_seg
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

            elif key == "" and not value.startswith("#"):
                ent[1] = delHTTPhost(value)


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

        for mime_type, slist in self.sub_list.items():
            if mime_list is not None and mime_type not in mime_list:
                continue

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
            for fn in man.get_seg_list(abs_paths=True):
                flist.append(fn)
        if flist != t["expected_list"]:
            print("FAILED unexpected list {}", flist)
            exit(-1)
        else:
            print("PASSED")
