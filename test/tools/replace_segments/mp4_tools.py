#!/usr/bin/python3
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
from datetime import datetime, timedelta
import logging
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET
from library.filesys_utils import *


def get_int32(data, off=0):
    """
    Extract a big-endian int32 from a byte array.
    """
    return (
        data[off + 0] * 0x1000000
        + data[off + 1] * 0x10000
        + data[off + 2] * 0x100
        + data[off + 3]
    )


def put_int32(value):
    """
    Convert an int into a big-endian int32 byte array.
    """
    return bytes(
        [
            value // 0x1000000,
            value // 0x10000 & 0xFF,
            value // 0x100 & 0xFF,
            value & 0xFF,
        ]
    )


def hex_dump(data, prefix="", width=32):
    """
    Print out a byte array as a combined hex and character report.
    """
    off = 0

    while len(data) > 0:
        seg = data[:width]
        seg_len = len(seg)
        chars = b""
        print(prefix, "      %5d: " % off, end="")

        for i in range(seg_len):
            b = seg[i]
            chars += bytes([b]) if b >= 32 and b < 127 else b"."
            print("%02x" % b, end=" ")

        if i < width:
            print("   " * (width - i), end="")
        print(chars.decode())
        data = data[seg_len:]
        off += width


def rep_int32(data, fld_list, flags=None, update=None):
    """
    Unpack a list of int32s from a byte array. This allows for fields being optional
    and to optionally update an object.
    """
    res = "     "
    off = 0

    if flags is None:
        flags = get_int32(data)
        if update is not None:
            setattr(update, "flags", flags)

        off = 4

    for name, flg in fld_list:
        if flags & flg == 0 and flg != 0:
            if update is None:
                continue

            setattr(update, name.rsplit("%", 1)[0], 0 if name.endswith("%d") else None)
            continue

        if name != "":
            amt = get_int32(data, off)
            # print(off, name, len(data), "0x%08x" % amt, amt)

            parts = name.rsplit("%", 1)
            if update is not None:
                setattr(update, parts[0], amt)

            form = "%%s=%%%s  " % (parts[1] if len(parts) > 1 else "d")
            res += form % (parts[0], amt)

        off += 4

    if update:
        update.last_off = off

    return res


def calc_off(off, fld_list, flags, srch_name=""):
    """
    Calculate the offset of a specific field in a byte array.
    """
    for name, flg in fld_list:
        if flags & flg == 0 and flg != 0:
            continue

        if name.startswith(srch_name) and srch_name != "":
            break

        off += 4

    return off


def out_int32(obj, fld_list, flags=None):
    """
    Convert the specified int32 fields from an object into a byte array.
    """
    res = bytes()

    if flags is None:
        flags = obj.flags
        res += put_int32(flags)

    for name, flg in fld_list:
        if flags & flg == 0 and flg != 0:
            continue

        if name != "":
            parts = name.rsplit("%", 1)
            res += put_int32(getattr(obj, parts[0]))
        else:
            res += bytes(4)

    return res


class mp4_segment(object):
    """
    Class to represent a MP4 file segment. These are formed from the input and then
    used separately to form the output.
    """

    mdat_read = 0
    mdat_write = 0

    def __init__(self, base_pts=0.0, logging=True):
        self.styp = None
        self.sidx = None
        self.moof = None
        self.mdat = None
        self.prft = None

        self.duration = None
        self.read_hddr = 0  # Flags for encountering boxes
        self.logging = logging
        self.base_pts = base_pts

    def parse(self, hddr_type, box_len=None, box=None):
        """
        When passed in another box, place the next entry indo the appropriate class.
        Encountering a 'mdat' box indicate that we have reach the end of a segment. If
        a class instance is already present, then it is treated as a concatination
        operation. This allows the segment to act as a FIFO buffer when reforming segments
        of specific sizes.

        The return from this call indicates as to whether we have reached the end of a
        segment and need to be called again with the same parameters to start a new
        segment.
        """
        if hddr_type is None:
            return self.end_of_blk()

        elif hddr_type == b"styp":
            if self.read_hddr & 0x01:
                return self.end_of_blk()

            self.read_hddr |= 0x01
            if self.styp is None:
                self.styp = box

        elif hddr_type == b"sidx":
            if self.read_hddr & 0x02:
                return self.end_of_blk()

            self.read_hddr |= 0x02

            if self.sidx is None:
                self.sidx = mp4_sidx(box, self.base_pts)

        elif hddr_type == b"moof":
            if self.read_hddr & 0x04:
                return self.end_of_blk()

            self.read_hddr |= 0x04

            if self.moof is None:
                self.moof = mp4_moof(
                    box, self.base_pts * 90000
                )  # A guess for the moment
            else:
                self.moof.concat(box)

        elif hddr_type == b"mdat":
            if self.read_hddr & 0x08:
                return self.end_of_blk()

            self.read_hddr |= 0x08
            mp4_segment.mdat_read += len(box)

            if self.mdat is None:
                self.mdat = box
            else:
                self.mdat += box

        elif hddr_type == b"styp":
            if self.read_hddr & 0x10:
                return self.end_of_blk()

            self.read_hddr |= 0x10
            self.prft = box

        return False

    def end_of_blk(self):
        """
        Handle reaching the end of a block/segment
        """
        self.read_hddr = 0

        if self.moof is None:
            return False

        if self.sidx is not None:
            self.moof.tfdt.set_time(self.sidx.timestamp)
            self.sidx.duration = self.moof.get_duration()
            self.duration = self.sidx.duration / self.sidx.timescale
            first_time = self.sidx.first_time

        else:
            self.duration = self.moof.get_duration() / 90000  # A guess for the moment
            first_time = 0

        if self.logging:
            log.info("Read segment:%d  backlog:%d",
                round(first_time, 3),
                round(self.duration, 3),
            )
        return True

    def split(self, targ_len):
        """
        Split off a new instance of this class with it containing just n seconds from
        the beginning. This class will retain the rest.
        """
        targ_len = round(targ_len * self.sidx.timescale)

        res = mp4_segment()
        res.styp = self.styp
        res.prft = self.prft
        if self.sidx is not None:
            res.sidx = mp4_sidx().copy(self.sidx)

        res.moof = self.moof.split(targ_len)
        sz = res.moof.get_size()
        res.mdat = self.mdat[:sz]
        self.mdat = self.mdat[sz:]

        if self.sidx is not None:
            res.sidx.duration = res.moof.get_duration()
            res.duration = duration = res.sidx.duration / res.sidx.timescale

            self.sidx.duration = self.moof.get_duration()
            self.duration = self.sidx.duration / self.sidx.timescale
            self.sidx.update_time(duration)
        else:
            self.duration = self.moof.get_duration() / 90000  # A guess for the moment

        return res

    def write(self, f):
        """
        Write content to an output file.
        """
        log.info("Write segment duration=%d", self.duration)

        moof = self.moof.write()
        mdat_len = len(self.mdat) + 8

        if self.styp is not None:
            f.write(put_int32(len(self.styp) + 8) + b"styp" + self.styp)

        if self.prft is not None:
            f.write(put_int32(len(self.prft) + 8) + b"prft" + self.prft)

        if self.sidx is not None:
            self.sidx.data_size = len(moof) + mdat_len
            f.write(self.sidx.write())

        f.write(moof)
        f.write(put_int32(mdat_len) + b"mdat" + self.mdat)

        mp4_segment.mdat_write += mdat_len - 8


class mp4_box(object):
    """
    Base class for representing a MP4 container box.
    """

    def __init__(self, box=None):
        if box is not None:
            rep_int32(box, self.field_lst, update=self)

    def copy(self, src):
        """
        Perform a copy of the variables to another instance of the class
        """
        for name in vars(src):
            setattr(self, name, getattr(src, name))

        return self

    def write(self):
        """
        Format into a byte array for writing to a file.
        """
        box = out_int32(self, self.field_lst)
        return put_int32(len(box) + 8) + self.box_id + box

    def __str__(self):
        return str(self.__dict__)


class mp4_prft(mp4_box):
    """
    Class to represent the 'sidx' entry.
    """

    box_id = b"prft"
    field_lst = [("Track_ID", 0), ("times", 0), ("timefp", 0)]

    def __init__(self, box=None):
        super().__init__(box)

        if box is not None:
            self.timestamp = float(self.times) + self.timefp / 0x100000000

    def update_time(self, to):
        """
        Update the timestamp.
        """
        self.times = int(to)
        self.timefp = int(to % 1 * 0x100000000)


class mp4_sidx(mp4_box):
    """
    Class to represent the 'sidx' entry.
    """

    box_id = b"sidx"
    field_lst = [
        ("refID", 0),
        ("timescale", 0),
        ("ptimeu%d", 0x01000000),
        ("ptimel", 0),
        ("offsetu%d", 0x01000000),
        ("offsetl", 0),
        ("count", 0),
        ("data_size", 0),
        ("duration", 0),
        ("sap%x", 0),
    ]

    def __init__(self, box=None, base_pts=0):
        super().__init__(box)

        if box is not None:
            self.timestamp = int(
                self.ptimeu * 0x100000000 + self.ptimel + base_pts * self.timescale
            )
            self.ptimeu, self.ptimel = divmod(self.timestamp, 0x100000000)
            self.first_time = self.timestamp / self.timescale

    def update_time(self, by):
        """
        Update the timestamp by moving it forwards.
        """
        self.first_time += by
        self.timestamp = round(self.first_time * self.timescale)
        self.ptimeu, self.ptimel = divmod(self.timestamp, 0x100000000)


class mp4_moof(mp4_box):
    """
    Class to represent the 'moof' entry and the various entries contained within..
    """

    def __init__(self, box=None, base_pts=0):
        self.base_pts = base_pts

        self.mfhd = None
        self.tfhd = None
        self.tfdt = None
        self.trun = None
        self.base_len = 8

        if box is not None:
            self.concat(box)

    def concat(self, box):
        """
        Add the contents of a 'moof' box into the class. This is called for both the inital
        fill and for any subsequent concatinations of following 'moof' boxes.
        """
        while len(box) > 0:
            hddr = box[:8]
            hddr_type = hddr[4:]

            if hddr_type == b"traf":
                box = box[8:]
                continue

            box_len = get_int32(hddr)

            if hddr_type == b"mfhd":
                mfhd = mp4_mfhd(box[8:])

                if self.mfhd is None:
                    self.mfhd = mfhd
                    self.base_len += box_len

            elif hddr_type == b"tfhd":
                tfhd = mp4_tfhd(box[8:])

                if self.tfhd is None:
                    self.tfhd = tfhd
                    self.base_len += box_len

                elif (
                    self.tfhd.Track_ID != tfhd.Track_ID
                    or self.tfhd.samp_duration != tfhd.samp_duration
                ):
                    log.error("self.tfhd.Track_ID=%s tfhd.Track_ID=%s",self.tfhd.Track_ID,tfhd.Track_ID)
                    log.error("self.tfhd.samp_duration=%s tfhd.samp_duration=%s",self.tfhd.samp_duration,tfhd.samp_duration)
                    log.error("box len %d",len(box))
                    #ffmpeg can produce a short last segment and in this case the sample duration will
                    #be less than has been read from previous segments. Skip generating an exception 
                    #for this case
                    if self.tfhd.Track_ID != tfhd.Track_ID:
                        raise Exception("In tfhd, Track_ID changed")

            elif hddr_type == b"tfdt":
                tfdt = mp4_tfdt(box[8:])
                tfdt.update_time(self.base_pts)

                if self.tfdt is None:
                    self.tfdt = tfdt
                    self.base_len += box_len

                elif self.tfdt.timestamp > tfdt.timestamp:
                    raise Exception(
                        "In tfdt, timestamps out of sequence %s:%s"
                        % (self.tfdt.timestamp, tfdt.timestamp)
                    )

            elif hddr_type == b"trun":
                trun = mp4_trun(self.tfhd, box[8:])

                if self.trun is None:
                    self.trun = trun
                else:
                    self.trun.concat(trun)

            box = box[box_len:]

    def get_duration(self):
        """
        Return duration described by the 'trun'
        """
        if self.tfhd.samp_duration is not None:
            return self.trun.count * self.tfhd.samp_duration

        return sum([ent.duration for ent in self.trun.entry_lst])

    def get_size(self):
        """
        Return size of data in 'mdat' that is descibed by the 'trun'.
        """
        return self.trun.calc_size()

    def split(self, targ_len):
        """
        Split the current 'moof' at the perscribed point. A new 'moof' will be
        created to contain the information upto the point. This will what will be
        returned.. The current 'moof' will containing the remaining part.
        """
        res = mp4_moof()

        res.mfhd = mp4_mfhd().copy(self.mfhd)
        res.tfhd = mp4_tfhd().copy(self.tfhd)
        res.tfdt = mp4_tfdt().copy(self.tfdt)
        res.trun = self.trun.split(targ_len, self.tfhd.samp_duration, self.base_len)

        if self.tfhd.samp_size is not None:
            self.trun.upd_samp_size()
            res.trun.upd_samp_size()

        self.tfdt.update_time(res.get_duration())
        self.mfhd.sequence += 1

        return res

    def write(self):
        """
        Format into a byte arry for writing to a file.
        """
        box = self.tfhd.write() + self.tfdt.write() + self.trun.write()
        box = self.mfhd.write() + put_int32(len(box) + 8) + b"traf" + box

        return put_int32(len(box) + 8) + b"moof" + box


class mp4_mfhd(mp4_box):
    """
    Class to represent the 'mfhd' entry.
    """

    box_id = b"mfhd"
    field_lst = [("sequence", 0)]


class mp4_tfhd(mp4_box):
    """
    Class to represent the 'tfhd' entry.
    """

    box_id = b"tfhd"
    field_lst = [
        ("Track_ID", 0),
        ("base_offset", 0x0001),
        ("samp_index", 0x0002),
        ("samp_duration", 0x0008),
        ("samp_size", 0x0010),
        ("samp_flags%x", 0x0020),
    ]


class mp4_tfdt(mp4_box):
    """
    Class to represent the 'tfdt' entry.
    """

    box_id = b"tfdt"
    field_lst = [("timeh", 0), ("timel", 0)]

    def __init__(self, box=None):
        super().__init__(box)

        if box is not None:
            self.timestamp = self.timeh * 0x100000000 + self.timel

    def update_time(self, by):
        """
        Update the timestamp by moving it forwards.
        """
        self.timestamp += by
        self.timeh, self.timel = divmod(self.timestamp, 0x100000000)

    def set_time(self, to):
        """
        Set the timestamp to the specific value.
        """
        self.timestamp = to
        self.timeh, self.timel = divmod(self.timestamp, 0x100000000)


class mp4_trun(mp4_box):
    """
    Class to represent the 'trun' entry and the table within it that describes
    the contents of the 'mdat' entry.
    """

    field_lst = [("count", 0), ("data_offset", 0x001), ("first_flags%x", 0x0004)]

    def __init__(self, tfhd=None, box=None):
        if tfhd is None:
            return

        self.tfhd = tfhd
        self.entry_lst = []

        super().__init__(box)
        flags = self.flags
        self.ent_sz = ent_sz = calc_off(0, self.trun_ent.field_lst, flags)

        if ent_sz <= 0:
            return

        for off in range(self.last_off, self.last_off + ent_sz * self.count, ent_sz):
            self.entry_lst.append(self.trun_ent(box[off : off + ent_sz], flags))

    def concat(self, src):
        """
        Concatinate the table entries from another 'trun' instance onto oneself. This
        process requires that the table entries are migrated towards having the most
        information. This may include adding the duration to each entry or building
        the table if not present.
        """
        self.ensure_flags()
        src.ensure_flags()

        # Same set of attributes
        if self.flags != src.flags:
            self.expand_table(src.flags)
            src.expand_table(self.flags)

        self.entry_lst += src.entry_lst
        self.count += src.count

    def ensure_flags(self):
        """
        Ensure that the table entries contain the 'flags' field. This is needed because
        concatinating and spliting 'trun' box means that I frames may be moved from the
        beginning to the middle of the table and this needs the presence of the 'flags'
        to indicate as such.
        """
        if self.flags & 0x0400 or self.first_flags is None:
            return

        if len(self.entry_lst) == 0:
            for num in range(self.count):
                ent = self.trun_ent(None, 0)
                ent.flags = self.tfhd.samp_flags
                self.entry_lst.append(ent)

        else:
            for ent in self.entry_lst:
                ent.flags = self.tfhd.samp_flags

        self.entry_lst[0].flags = self.first_flags
        self.flags |= 0x0400
        self.ent_sz += 4

        self.flags &= ~0x0004
        self.last_off -= 4

    def expand_table(self, targ_flags):
        """
        In order to merge two 'trun' entries together, it is necessary to ensure that
        the table is build and also contains the common set of items. This will expland
        the table based upon the target set of flags.

                field_lst = [("duration", 0x0100), ("size", 0x0200), ("flags%x", 0x0400), ("time_offset", 0x0800)]
        """
        self.ent_sz = calc_off(0, self.trun_ent.field_lst, self.flags | targ_flags)
        ent_dur = self.tfhd.samp_duration
        ent_size = self.tfhd.samp_size

        if len(self.entry_lst) == 0:
            self.flags |= targ_flags

            for num in range(self.count):
                ent = self.trun_ent(None, 0)
                self.entry_lst.append(ent)

                if self.flags & 0x0100:
                    ent.duration = ent_dur
                if self.flags & 0x0200:
                    ent.size = ent_size

            return

        diff_flags = (self.flags ^ targ_flags) & ~self.flags & 0x0300
        if diff_flags == 0:
            return

        for ent in self.entry_lst:
            if diff_flags & 0x0100:
                ent.duration = ent_dur
            if diff_flags & 0x0200:
                ent.size = ent_size

        self.flags |= targ_flags

    def write(self):
        """
        Format into a byte array for writing to a file.
        """
        box = out_int32(self, self.field_lst)

        for ent in self.entry_lst:
            box += ent.write(self.flags)

        return put_int32(len(box) + 8) + b"trun" + box

    def split(self, targ_len, samp_duration, base_len):
        """
        Split the current 'trun' at the perscribed point. A new 'trun' will be
        created to contain the information upto the point. This will what will be
        returned.. The current 'trun' will containing the remaining part.
        """
        self.ensure_flags()

        if samp_duration is not None:
            tab_cnt = round(targ_len / samp_duration)

        else:
            tab_cnt = 0

            for ent in self.entry_lst:
                targ_len -= ent.duration
                if targ_len <= 0:
                    break

                tab_cnt += 1

        if tab_cnt >= self.count:
            tab_cnt = self.count - 1

        res = mp4_trun().copy(self)
        res.entry_lst = res.entry_lst[:tab_cnt]
        res.count = tab_cnt

        self.entry_lst = self.entry_lst[tab_cnt:]
        self.count -= tab_cnt

        res.adj_data_off(base_len)
        self.adj_data_off(base_len)

        return res

    def adj_data_off(self, base_len):
        """
        Adjust the offset from the base of the 'moof' to the following 'mdat'.
        """
        self.data_offset = (
            base_len + 24 + self.last_off + self.ent_sz * len(self.entry_lst)
        )

    def upd_samp_size(self):
        """
        Update the sample size from the first entry in the table.
        """
        if self.flags & 0x0200 and len(self.entry_lst) > 0:
            self.tfhd.samp_size = self.entry_lst[0].size

    def calc_size(self):
        """
        Calculate the amount of the following 'mda' that is described by the 'trun' box.
        """
        if self.flags & 0x0200:
            return sum([ent.size for ent in self.entry_lst])

        return self.tfhd.samp_size * self.count

    class trun_ent(object):
        """
        Table entry class for containing a frame descriptions
        """

        field_lst = [
            ("duration", 0x0100),
            ("size", 0x0200),
            ("flags%x", 0x0400),
            ("time_offset", 0x0800),
        ]

        def __init__(self, box, flags):
            if box is not None:
                rep_int32(box, self.field_lst, flags, update=self)

        def write(self, flags):
            return out_int32(self, self.field_lst, flags)


do_run = True


def input_mp4_box(flist):
    """
    Generator function to read the MP4 box entries from a list of files. If
    a list of files is supplied, they are treated as being concatinated together.
    """
    global do_run

    if type(flist) is not list:
        flist = flist.split(",")

    for fn in flist:
        if fn != "-":
            f = open(fn, "rb")
            log.info("Reading file %s", fn)
            read_func = f.read
        else:
            f = sys.stdin
            read_func = f.buffer.read

        while do_run:
            hddr = read_func(8)
            if len(hddr) < 8:
                break

            box_len = get_int32(hddr)
            hddr_type = hddr[4:]

            # print("  ", box_len, hddr_type)
            yield hddr_type, box_len, read_func(box_len - 8)

        f.close()


class output_mp4_seg(object):
    """
    Generate output segment files using mp4_segment as a fifo buffer. This requires
    a generator to supply the list of durations into which the output is to be split
    and another generator to supply the output file names.
    """

    def __init__(
        self, dur_parm, output_parm, do_init=True, do_flush=False, base_pts=0.0
    ):
        self.duration = 0
        self.fout = None
        self.cur_seg = None
        self.do_init = do_init
        self.do_flush = do_flush

        self.dur_iter = iter(dur_parm)
        self.duration = next(self.dur_iter)

        self.output_parm = output_parm
        self.out_iter = iter(output_parm)
        self.base_pts = float(base_pts)
        self.flist = []

        if do_init:
            self.open_out()

    def open_out(self):
        """
        Open a new instance of an output file. If the template does not contain
        a substitution parameter then this will null out and the same file will
        be left open.
        """
        if self.fout is not None:
            # if not self.output_parm.mult_file: return
            self.fout.close()
            log.info("Finished writing %s", self.flist[-1])

        fn = self.output_parm.get_init_fn() if self.do_init else next(self.out_iter)
 
        log.info("Open for write %s",fn)
        self.fout = open(fn, "wb")
        self.flist.append(fn)

    def copy_file(self, fin):
        """
        Add a mp4 box into the currently buffered segment. Then see if it
        is time to flush out the contents to the output file. The copy terminates
        when either the imput is exhausted or one of the generators indicates that
        no more output is required.
        """
        fin = iter(fin)
        self.cur_seg = mp4_segment(self.base_pts)

        if self.do_init:
            # Output any header boxes until segment start
            for hddr_type, box_len, box in fin:
                if hddr_type in [b"styp", b"moof", b"sidx", b"prft"]:
                    break

                self.fout.write(put_int32(box_len) + hddr_type + box)

            self.do_init = False

        else:
            hddr_type, box_len, box = next(fin)

        # Process all remaining boxes in the media file until no more or no more durations
        while True:
            try:
                # Add boxes until next segment encountered then retry with same box for start of next
                while not self.cur_seg.parse(hddr_type, box_len, box):
                    hddr_type, box_len, box = next(fin)

                if self.duration > 0.5:
                    self.split()
                else:
                    self.open_out()
                    self.cur_seg.write(self.fout)
                    self.cur_seg = mp4_segment()

            except StopIteration:
                break

        if self.duration >= 0.5:
            try:
                self.cur_seg.parse(None)
                self.split()

                if self.do_flush:
                    self.open_out()
                    self.cur_seg.write(self.fout)

            except StopIteration:
                pass

        if self.fout is None:
            return self

        self.fout.close()
        log.info("Finished writing %s", self.flist[-1])
        self.fout = None

        log.debug("Written %d list %s", len(self.flist), self.flist)
        return self

    def split(self):
        """
        Split buffer according to the list of durations. When exhausted, the
        last one is used.
        """
        while self.cur_seg.duration > self.duration:
            self.open_out()
            self.cur_seg.split(self.duration).write(self.fout)

            self.duration = -1
            self.duration = next(self.dur_iter)


class duration_list(object):
    """
    Generator class for returning the list of durations from the supplied parameter.
    When exhuasted, the last duration is optionally returned each time.
    """

    def __init__(self, parm, stop_end=False):
        self.stop_end = stop_end

        if type(parm) is list:
            self.duration = parm

        else:
            parts = parm.split(",")

            if parts[0] == "":
                self.duration = [0.0]
            else:
                self.duration = [float(tm) for tm in parts]

    def __iter__(self):
        while True:
            yield self.duration[0]

            if len(self.duration) > 1:
                self.duration = self.duration[1:]
            elif self.stop_end:
                break


class transcode_flist(object):
    """
    Base class to supply the list of output files and durations for the do_transcoding()
    function. This serves up a list of output files and matching durations. These
    are then supplied to the class output_mp4_seg for its processing.
    """

    def __init__(self, out_dir):
        self.init_file = None
        self.missing = 0

        self.flist = []
        self.dur_list = []
        self.tot_duration = 0.0

        if not out_dir:
            self.out_dir = "./out/"
        else:
            self.out_dir = out_dir if out_dir.endswith("/") else out_dir + "/"

    def get_init_fn(self):
        """
        Return the initialisation output file name.
        """
        return self.init_file

    def __iter__(self):
        """
        Return a generator that returns the segement output file names.
        """
        for fn in self.flist:
            yield fn

    def durations(self):
        """
        Return a generator that returnes the segment durations.
        """
        return duration_list(self.dur_list, False)

    def split_points(self):
        """
        Return a list positions at which the media file is split.
        """
        res = []
        duration = 0

        for dur in self.dur_list:
            duration += dur
            res.append(str(round(duration, 3)))

        return res

    def get_header_codec(self, url):
        subp = subprocess.Popen(
            [
                f"cat {url} | ffprobe -hide_banner -show_format -show_streams -count_frames -pretty -of xml - 2>/dev/null"
            ],
            shell=True,
            stdout=subprocess.PIPE,
        )
        tree = ET.parse(subp.stdout)
        root = tree.getroot()
        codec = ""
        for elem in root:
            if elem.tag.endswith("streams"):
                for stream in elem:
                    codec = stream.get("codec_name")
        log.info(f"header codec over-ride -> {codec} | cat {url} | ffprobe -hide_banner -show_format -show_streams -count_frames -pretty -of xml - 2>/dev/null")
        subp.wait()
        return codec

    def media_attribs(self, attrs=None):
        """
        Return the attributes for the target based upon the imput files.
        """
        ffprobe_on = ""
        if self.init_file is None:
            subp = subprocess.Popen(
                [
                    "ffprobe -hide_banner -show_format -show_streams -count_frames -pretty -of xml %s 2>/dev/null"
                    % (self.flist[0])
                ],
                shell=True,
                stdout=subprocess.PIPE,
            )
            ffprobe_on = self.flist[0]
        else:
            subp = subprocess.Popen(
                [
                    "cat %s %s | ffprobe -hide_banner -show_streams -count_frames -show_format -pretty -of xml - 2>/dev/null"
                    % (self.init_file, self.flist[0])
                ],
                shell=True,
                stdout=subprocess.PIPE,
            )
            ffprobe_on = self.init_file

        header_codec = self.get_header_codec(ffprobe_on)
        
        tree = ET.parse(subp.stdout)
        root = tree.getroot()

        rates = {}  # Bit rates for each stream
        codecs = {}  # Codec for each stream

        fps = ""  # Frame rate for Video
        dim = ""  # Screen dimensions for Video
        pts = 0  # Start PTS for the streams
        time_base = 90000  # Timebase to convert PTS to seconds.
        numb_frames = 0  # Number of frames in segment

        attr_rate = None
        attr_fps = "25"
        attr_dim = ""

        if attrs is not None and hasattr(attrs, "TYPE") and hasattr(attrs, "BANDWIDTH"):
            log.info("attrs %s", attrs)
            attr_rate = int(attrs.BANDWIDTH) / 1000
            if hasattr(attrs, "FPS"):
                attr_fps = attrs.FPS
            if hasattr(attrs, "RESOLUTION"):
                attr_dim = attrs.RESOLUTION

        for elem in root:
            if elem.tag.endswith("streams"):
                for stream in elem:
                    log.info("stream.attrib %s", stream.attrib)
                    med_type = stream.get("codec_type")
                    rates[med_type] = stream.get(
                        "bit_rate", rates.get(med_type, "100 Kbit/s")
                    )
                    codecs[med_type] = stream.get("codec_name", "")

                    # DELIA-62747 : patch to over-ride codec of segment with codec of header
                    if header_codec not in ["", None]:
                        codecs[med_type] = header_codec

                    if med_type == "video":
                        parts = stream.get("r_frame_rate", attr_fps).split("/", 1)
                        fps = int(parts[0])
                        if len(parts) > 1:
                            fps /= int(parts[1])

                        fps = str(max(min(fps, 100), 10))
                        if "nb_read_frames" in stream.attrib:
                            numb_frames = int(stream.get("nb_read_frames"))

                        dim = stream.get("width", "")
                        if dim != "":
                            dim += "x" + stream.get("height", None)

                    if "start_pts" in stream.attrib:
                        pts = int(stream.get("start_pts"))

                    if "time_base" in stream.attrib:
                        parts = stream.get("time_base").split("/", 1)
                        time_base = int(parts[1])

            elif elem.tag.endswith("format"):
                brate = elem.get("bit_rate", None)

                if brate is not None:
                    rates["video"] = brate
                    codecs["video"] = ""

        if len(rates) > 0:
            log.info("rates %s", rates)

        elif attrs is None:
            log.error("Could not identify media rates for audio/video from segment files")
            exit(1)

        elif attr_rate is not None:
            log.info("Resolving from manifest:%s", attrs)
            med_type = attrs.TYPE
            rates[med_type] = "%d Kbit" % attr_rate
            codecs[med_type] = ""

            if med_type == "video":
                fps = attr_fps
                dim = attr_dim

        else:
            log.error("Could not identify media rates for audio/video from manifest")
            exit(1)

        if numb_frames == 1:
            fps = "1"

        for med_type in rates:
            parts = rates[med_type].split(None, 1)
            rate = float(parts[0])

            if len(parts) > 1:
                if "Mbit" in parts[1]:
                    rate *= 1000.0

                elif attr_rate is not None and (
                    "Kbit" not in parts[1] or rate < attr_rate * 0.9
                ):
                    log.warning("Probable rubbish bit rate %s overriding with %d",
                        parts,
                        attr_rate,
                    )
                    rate = attr_rate

            elif attr_rate is not None:
                rate = attr_rate

            rate = rates[med_type] = str(round(rate))

            log.info(
                "Input type:%s bit rate:%s Codec:%s FPS:%s Dimensions:%s pts:%d timebase %d",
                med_type,
                rate,
                codecs[med_type],
                fps,
                dim,
                pts,
                time_base,
            )

        subp.wait()
        # DELIA-62747 : temparary patch to handle ac3 codec of segment -> replace {'audio': 'ac3'} with {'audio': 'eac3'}
        # if "ac3" == codecs.get('audio',''):
        #     codecs['audio'] = 'eac3'

        return rates, codecs, str(round(pts / time_base, 3)), fps, dim


class mpeg_flist(transcode_flist):
    """
    Derived class to build the list of output files and their duration by reading
    a list of mp4 or transport segment files.
    """

    def __init__(self, flist, out_dir=None, chek_init=True, force_probe=False):
        super().__init__(out_dir)
        self.chek_init = chek_init

        if force_probe:
            self.use_ffprobe(flist)

        else:
            for fn in flist:
                if not fn.endswith(".ts"):
                    continue

                self.use_ffprobe(flist)
                break

            else:
                self.use_mp4_parser(flist)

        self.tot_duration = round(self.tot_duration, 3)
        log.info("Total duration of %d files:%d list %s",
            len(self.flist),
            self.tot_duration,
            self.dur_list,
        )

    def use_mp4_parser(self, flist):
        """
        Use an internal parser to identify the duration from each file in the list.
        """
        for fn in flist:
            # print('+'*20, fn, '+'*20)

            try:
                duration = 0
                seg = mp4_segment(logging=False)

                for hddr_type, box_len, box in input_mp4_box(fn):
                    while seg.parse(hddr_type, box_len, box):
                        duration += seg.duration
                        seg = mp4_segment(logging=False)

                seg.parse(None)
                if seg.duration:
                    duration += seg.duration

                seg = None

                if duration < 0.5:
                    self.init_file = fn
                    continue

                duration = round(duration, 3)
                self.flist.append(fn)
                self.dur_list.append(duration)
                self.tot_duration += duration

            except FileNotFoundError:
                self.missing += 1

        if (
            (self.init_file is None or len(self.flist) < 1)
            and self.tot_duration > 0
            and self.chek_init
        ):
            log.error("Cannot identify 'init' file or first non 'init' file from the list")
            exit(1)

    def use_ffprobe(self, flist):
        """
        Use ffprobe to identify the the duration from each file in the list.
        """
        for fn in flist:
            log.info("fn %s",fn)

            try:
                stat = os.stat(fn)

                subp = subprocess.Popen(
                    [
                        "ffprobe -hide_banner -show_format -pretty -of xml %s 2>/dev/null"
                        % (fn)
                    ],
                    shell=True,
                    stdout=subprocess.PIPE,
                )

                tree = ET.parse(subp.stdout)
                root = tree.getroot()

                for elem in root:
                    if not elem.tag.endswith("format"):
                        continue

                    tm = elem.get("duration").split(":")
                    duration = round(
                        int(tm[0]) * 3600 + int(tm[1]) * 60 + float(tm[2][:-1]), 3
                    )

                    self.flist.append(fn)
                    self.dur_list.append(duration)
                    self.tot_duration += duration

                    break

                subp.wait()

            except ET.ParseError:
                subp.wait()
                self.missing += 1
                log.error("Failed parsing XML from ffprobe %s", fn)

            except FileNotFoundError:
                self.missing += 1

        if len(self.flist) < 1 and self.chek_init:
            log.error("Cannot identify any valid tranport segment files")
            exit(1)


WORK_DIR = ""


def set_workdir():
    global WORK_DIR
    if WORK_DIR != "":
        return

    WORK_DIR = os.environ["MPEG_WORKDIR"] if "MPEG_WORKDIR" in os.environ else ""
    if WORK_DIR == "":
        os.environ["MPEG_WORKDIR"] = WORK_DIR = os.getcwd() + "/tmp"


def do_transcode(base_dir, proc_list, skeleton, attrs=None, no_trans=False):
    """
    Given an class trancode_flist, use this as a template for generating replacement
    media files from a skeleton media file base upon bit rates etc. Then resegment
    the output files to match the originals.
    """
    if proc_list.tot_duration < 0.5:
        return

    set_workdir()
    filetype_ts = proc_list.init_file is None
    rates, codecs, pts, fps, dim = proc_list.media_attribs(attrs)
    # return  # Comment out
    # no_trans = True  # Comment out

    iframe_pos = ",".join(proc_list.split_points())

    if no_trans:
        pass

    elif filetype_ts:
        subp = subprocess.Popen(
            [
                "%s/segment_both.sh" % (base_dir),
                skeleton,
                iframe_pos,
                pts,
                rates.get("video", ""),
                codecs.get("video", ""),
                dim,
                rates.get("audio", ""),
                codecs.get("audio", ""),
                fps,
            ],
            shell=False,
        )
        subp.wait()

    else:
        for med_type in rates:
            display_text="Pid{} {}k".format(getattr(attrs,"period_id","?") ,rates[med_type])
            log.info("Starting transcoding from:%s", skeleton)
            subp = subprocess.Popen(
                [
                    "%s/segment_%s.sh" % (base_dir, med_type),
                    skeleton,
                    str(proc_list.tot_duration + 1),
                    rates[med_type],
                    codecs[med_type],
                    pts,
                    dim,
                    fps,
                    iframe_pos,
                    display_text
                ],
                shell=False,
            )
            subp.wait()

    tmp_flist = os.listdir(WORK_DIR)
    tmp_flist.sort()
    skel_flist = ""

    for fn in tmp_flist:
        if fn.startswith("init-stream0"):
            skel_flist = "," + WORK_DIR + "/" + fn + skel_flist
        elif fn.startswith("chunk-stream0"):
            skel_flist += "," + WORK_DIR + "/" + fn

    if not skel_flist:
        log.error("Could not locate files in tmp directory from ffmpeg")
        exit(1)

    skel_flist = skel_flist[1:]
    log.info("Resegmenting the following list of files:%s", skel_flist.replace(",", "  "))

    if filetype_ts:
        out_iter = iter(proc_list)

        try:
            for fn in skel_flist.split(","):
                copy_file(fn, next(out_iter))
        except StopIteration:
            pass

    else:
        seg_out = output_mp4_seg(proc_list.durations(), proc_list, base_pts=pts)
        seg_out.copy_file(input_mp4_box(skel_flist))

    log.info("Transcoding finished")

log = logging.getLogger('root')


if __name__ == "__main__":
    base_dir = os.path.dirname(sys.argv[0])

    fn = "test/segment_1.ts"
    subp = subprocess.Popen(
        ["ffprobe -hide_banner -show_format -pretty -of xml %s 2>/dev/null" % (fn)],
        shell=True,
        stdout=subprocess.PIPE,
    )

    tree = ET.parse(subp.stdout)
    root = tree.getroot()

    print(root.tag, root.attrib)

    for sub in root:
        print("  ", sub.tag, sub.attrib)

    subp.wait()
