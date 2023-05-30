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
import os
import sys
import json
from datetime import *
import time
import re
from enum import Enum
from pathlib import Path


def delHTTP(line):
    result = re.sub(r"^https?://", "", line)
    return result


def delHTTPhost(line):
    result = re.sub(r"^https?://.+?/", "", line)
    return result


def write_harvest_details():
    """
    Record some useful details about this harvest
    """
    time_str = datetime.utcnow().isoformat()
    data = {"recording_start_time": time_str, "args": sys.argv}
    with open("harvest_details.txt", "w") as f:
        print(json.dumps(data))
        f.write(json.dumps(data))


def read_harvest_details():
    d = {}
    with open("harvest_details.txt") as f:
        d = json.load(f)

    return d


class Manifest(object):
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
        """
        if not base:
            base = self.orig_path
        elif base.startswith("http://") or base.startswith("https://"):
            pass
        elif not base.startswith('/') and '/' in self.orig_path:
            base = self.orig_path.rsplit('/', 1)[0] + '/' + base

        if fn.startswith("https://"):
            return fn[7:]
        if fn.startswith("http://"):
            return fn[6:]

        dn = base.rsplit("/", 1)[0] if "/" in base else ""

        while fn.startswith("../"):
            dn = dn.rsplit("/", 1)[0] if "/" in dn else ""
            fn = fn.split("/", 1)[1]

        return fn if dn == "" else dn + "/" + fn

    def rept_bands(self):
        res = {}

        for band, mime_type in self.bands.items():
            res.setdefault(mime_type[0], []).append(band)

        return res

class seg_list_base(object):
    """
    Base interface class for building the list of segment files from geg_seg_list() calls to
    a manifest class.
    """
    def __init__(self):
        self.slist = []
        self.play_order=0

    def play_order_reset(self):
        self.play_order=1

    def new_file(self, key, encrypted, attrs=None):
        """
        Call for when a new segmented file is encountered.
        """
        pass

    def add_init(self,fn):
        """
        Call for when a stream initialisation file is to be atted to the list.
        """
        self.play_order=0
        self.add_file(fn,0)

    def add_file(self, fn, duration):
        """
        Call for when a segment needs to be added.
        """
        self.slist.append({'seg':fn,'play_order':self.play_order})
        self.play_order+=1

    def __iter__(self):
        """
        Return an iterator over the resultant list.

        The list is returned after sorting on 2 parameters:
        1) sorted by the play_order value so that segments played first are returned first
        2) sorted alphabetically so that for example '480p_003.m4s' is always before '720p_003.m4s'
        """

        l = sorted(self.slist,key=lambda x: (x.get('play_order'),x.get('seg')))

        #Extract just seg field from sorted list
        self.rtn_list=[]
        for seg in l:
            self.rtn_list.append(seg.get('seg'))

        return iter(self.rtn_list)


def get_incr_manifests(man_path_list, oper=lambda x: x):
    """
    Obtain the list of incremental manifest files that have been downloaded
    into the directory hierachy and return them as a correctly sorted list
    based upon the numeric suffix.
    """
    res = []

    for path in [man_path_list] if type(man_path_list) is str else man_path_list:
        flist = []

        dn = os.path.dirname(path)
        dn = "./" if dn == "" else dn + "/"
        prefix = os.path.basename(path)

        for fn in os.listdir(dn):
            if not fn.startswith(prefix) or "." not in fn or fn == prefix:
                continue

            main, idx = fn.rsplit(".", 1)
            if not idx.isdecimal():
                continue

            flist.append((int(idx), dn + fn))

        flist.sort(key=lambda e: e[0])
        res.append((path, [oper(fn) for idx, fn in flist]))

    return res


def coalesce_manifests(man_path_list, man_type, max_merge=0):
    """
    Build a single coalesced manifest by merging the list of
    individual incremental manifests that were written over time.
    """
    man_list = []

    if len(man_path_list) > 0 and type(man_path_list[0]) is not tuple:
        man_path_list = get_incr_manifests(man_path_list)

    for path, flist in man_path_list:
        cnt = 0
        first_man = None

        merged = max_merge
        print("Merging %s: " % path, end="")

        for fn in flist:
            print(".", end="", flush=True)
            man = man_type(fn)
            cnt += 1

            if first_man:
                first_man.do_merge(man)
                merged -= 1
                if merged == 0:
                    break
            else:
                man.orig_path = path
                first_man = man

        if first_man is None and Path(path).is_file():
            first_man = man_type(path)
            print(" no incrementals but single manifest found")
        elif cnt > 0:
            print("\n  %d manifests processed" % cnt)
        else:
            print(" no manifests found")

        if first_man is not None:
            man_list.append(first_man)

    return man_list


def single_manifest(manfn, man_type):
    """
    Return a single manifest instance. If the pointed to is an incremental
    manifest, then each file will be coalesced together.
    """
    exists = Path(manfn).is_file()

    if exists and not manfn.endswith(".1"):
        man = man_type(manfn)

    else:
        if exists:
            manfn = manfn[:-2]
        man_lst = coalesce_manifests(manfn, man_type)

        if len(man_lst) != 1:
            print("No manifest files found for", manfn)
            return None

        man = man_lst[0]
        man.orig_path = manfn

    return man


def get_seg_manlist(
    manfn, man_type, from_seg=None, abs_paths=False, seg_list=None, oper=None
):
    """
    Returns the segment files from a manfest. If the manifest is not a single
    files, then attempt to expand from an incremental set of manifest files.
    """
    if from_seg is None:
        from_seg = {}
    if seg_list is None:
        seg_list = seg_list_base()

    man = single_manifest(manfn, man_type)

    if man is not None:
        if oper is not None:
            oper(man)

        man.get_seg_list(from_seg=from_seg, abs_paths=abs_paths, seg_list=seg_list)

    return seg_list


if __name__ == "__main__":
    base_dir = os.path.dirname(sys.argv[0])
    os.chdir(base_dir + "/..")

    time.sleep(1)
    print("no tests at the moment")

