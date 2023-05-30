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
import os
import sys
import argparse
from pathlib import Path

def locate_paths(from_dir, srch=None, ends=None, starts=None):
    """
    Return a list of directories with their assocated files that
    match the supplied critera.
    """
    res = []

    for path, dirs, files in os.walk(from_dir):
        if path.endswith("/orig_bak"): continue

        mfiles = set()

        if srch: mfiles.update([ fn for fn in files if srch in fn ])
        if ends: mfiles.update([ fn for fn in files if fn.endswith(ends) ])
        if starts: mfiles.update([ fn for fn in files if fn.startswith(starts) ])

        if mfiles: res.append((path, list(mfiles)))

    return res

def backup_file(fn, force=False):
    """
    Take a backup of an output file. This only done if a backup does not exist.
    """
    if not Path(fn).is_file(): return

    dn = os.path.dirname(fn)
    fn = os.path.basename(fn)

    dn = "./" if dn == "" else dn + '/'
    bak_dir = dn + "orig_bak/"

    try:
        os.mkdir(bak_dir)
    except FileExistsError:
        pass

    if not force and Path(bak_dir + fn).is_file(): return

    copy_file(dn + fn, bak_dir + fn, False)

def copy_file(in_fn, out_fn, do_backup=True):
    """
    Do a binary copy of a file.
    """
    fin = open(in_fn, "rb")
    if do_backup: backup_file(out_fn)

    fout = open(out_fn, "wb");

    while True:
        blk = fin.read()
        if len(blk) <= 0: break

        fout.write(blk)

    fin.close()
    fout.close()
    print("  Finished writing", out_fn)

def restore_files(in_dir, do_list=True):
    """
    Restore all files within a direct structure using backups stored in
    the orig_bak sub directories.
    """
    cnt = 0
    if in_dir is None: in_dir = '.'

    for path, dirs, files in os.walk(in_dir):
        if not path.endswith("/orig_bak"): continue

        for fn in files:
            src_fn = path + '/' + fn
            dest_fn = path[:-8] + fn
            cnt += 1

            if do_list:
                print("In", path[:-9], "backup of", dest_fn, "from", src_fn)
            else:
                copy_file(src_fn, dest_fn, False)

    print("Restored", cnt, "files")
    return cnt

def copy_flist(flist, src_dir, dest_dir):
    """
    Copy a list of files from the Source to the destination directory
    """
    print("Number of files to copy:", len(flist))
    if not src_dir.endswith('/'): src_dir += '/'
    if not dest_dir.endswith('/'): dest_dir += '/'

    copied = 0
    missing = 0
    skipped = 0

    for src_fn in flist:
        src_fn, obj = src_fn if type(src_fn) is tuple else ( src_fn, None )

        if not src_fn.startswith(src_dir):
            skipped += 1
            continue

        dest_fn = dest_dir + src_fn[len(src_dir):]
        #print("copy", src_fn, "to", dest_fn)

        parts = dest_fn.rsplit('/', 1)
        if len(parts) >1: os.makedirs(parts[0], exist_ok=True)

        if obj is None:
            try:
                copy_file(src_fn, dest_fn, False)
                copied += 1
            except FileNotFoundError:
                missing += 1

        else:
            with open(dest_fn, "w") as f:
                f.write(str(obj))

            copied += 1

    print("Copied:", copied, "Missing:", missing, "Skipped:", skipped)
    return copied

if __name__ == "__main__":
    base_dir = os.path.dirname(sys.argv[0])

    restore_files(None)

