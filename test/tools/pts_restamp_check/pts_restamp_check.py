#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Synamedia Ltd.
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

import fileinput
import re

counts = {}
timescales = {}
pending_inject = {}
pts_offset = {}
MEDIA = ['video', 'audio', 'subtitle']
media_lookup = { } #{ url1: 'video' , url2: 'audio' }

def init():
    for med in ['audio', 'video', 'subtitle']:
        counts.setdefault(med, [])
        timescales.setdefault(med, 1)
        pts_offset.setdefault(med, 0)

init()
for line in fileinput.input(errors="ignore"):
    timestamp=1
    m = re.match(r'(.*?Z)',line)
    if m:
        timestamp=m.group(1)
    # Look for log line to indicate a tune has taken place
    m = re.search(r'(aamp_tune: .*)', line)
    if m:
        tune = m.group(1)
        for media in ['audio', 'video', 'subtitle']:
            counts[media].append({'timestamp': timestamp, 'msg': f'Tune {tune}'})

    m = re.search(r'(.*\[Flush\])', line)
    if m:
        msg = m.group(1)
        for media in ['audio', 'video', 'subtitle']:
            counts[media].append({'timestamp': timestamp, 'msg': 'Flush','didFlush': True})
    m = re.search(r'GST_MESSAGE_ERROR', line)
    if m:
        for media in ['audio', 'video', 'subtitle']:
            counts[media].append({'timestamp': timestamp, 'msg': 'GST_MESSAGE_ERROR'})

    # sendHelper where segments get injected
    m = re.search(r'SendHelper.*?Sending segment for mediaType\[(\d)\].*init:(\d)', line)
    if m:
        media_num = int(m.group(1))
        media = MEDIA[media_num]
        is_init = int(m.group(2))
        if is_init:
            counts[media].append({'timestamp': timestamp, 'msg': 'init segment'})
        elif media in pending_inject and pending_inject[media]:
            counts[media].append(pending_inject[media])
            pending_inject[media] = None
        else:
            counts[media].append({'timestamp': timestamp, 'msg': 'Trickplay ?'})

    # Get the timescale from the log line that should preceed IsoBmffHelper::RestampPts()
    # video timeScale 240000 mPTSOffsetSec -843722.237242
    m = re.search(r'(?:ProcessFragmentChunk|ProcessAndInjectFragment).*(audio|video|subtitle) timeScale (\d+) mPTSOffsetSec ([-+]?\d+\.\d+)', line)
    if m:
        media = m.group(1)
        ts = int(m.group(2))
        pts_offset[media] = m.group(3) # pts offset same regardless of media
        timescales[media] = ts
        last_media = media

    # Build up a table so we know the media type of each segment url
    m = re.search(r'CacheFragment.*Type\[(\d)\].*(http.*)',line)
    if m:
        num = int(m.group(1))
        if num <3:
            media = MEDIA[num]
            url = m.group(2)
            media_lookup.update({url: media})

    # Read the restamp logline
    m = re.search(r'RestampPts.*before (\d+) after (\d+) duration (\d+) (.*)',line)
    if m:
        before = int(m.group(1))
        after = int(m.group(2))
        log_duration = int(m.group(3))
        url = m.group(4)

        # Get segment number from URL if possible
        n = re.search(r'(\d+)\.(mp4|seg)', url)
        seg_num = 0
        if n:
            seg_num = int(n.group(1))

        # Get video profile I.E bitrate from URL if possible. Works for sky
        p = re.search(r'_(audio\d+|video\d)-', url)
        if p:
            profile = p.group(1)
        else:
            profile = '?'

        # Lookup url in table to get media type
        if url in media_lookup:
            media = media_lookup.get(url)
        else:
            print("Unknown url ",url)

        # The segment has been restamped but it is only injected when it gets to sendHelper.
        # IsoBmffProcessor holds init segment until following video segment is received. The
        # following stops it appearing that the order has been reversed
        pending_inject[media] = {'timestamp': timestamp,
                              'before': before,
                              'after': after,
                              'timescale': timescales[media],
                              'mPTSOffsetSec': pts_offset[media],
                              'log_duration': log_duration,
                              'url': url,
                              'seg_num': seg_num,
                              'profile': profile}

# Finished reading log files(s) output as CSV

# excel column headings
# The first two columns for easy excel graph generation
# showing the increasing pts time in seconds
print('Timestamp (from log),', end="")
print('PTS after/timescale Seconds (calculated),', end="")
print('Media,', end="")
print('Original PTS in seg (from log),', end="")
print('PTS after restamp (from log),', end="")
print('timescale (from log),', end="")
print('PTS Offset(s) (from log),', end="")
print('PTS(n) - PTS(n+1) I.E duration (calculated),', end="")
print('duration read from segment (from log),', end="")
print('seg_num (from url),', end="")
print('profile (from url),', end="")
print('url (from log)')

for media in ['video', 'audio', 'subtitle']:

    stream = counts[media]
    for idx, entry in enumerate(stream):
        print(f"{entry['timestamp']},",end="")

        if 'msg' in entry:
            print(f",{media},{entry['msg']}")
            previous = 0
            continue

        # duration of this segment is pts(n+1) - pts(n)
        didFlush = False
        for idx1 in range(idx+1,len(stream)):
            if 'didFlush' in stream[idx1]:
                didFlush = True
            if 'after' in stream[idx1]:
                break

        seg_duration = 0
        if didFlush:
            seg_duration = '? flush follows' # Cannot calculate the duration of last segment before a flush
        elif idx1<len(stream) and 'after' in stream[idx1]:
            next_entry = stream[idx1]
            seg_duration = next_entry['after']*entry['timescale']/next_entry['timescale'] - entry['after']

        offset_applied = entry['after'] - entry['before']
        pts_seconds = entry['after']/entry['timescale']
        print(f"{pts_seconds:.6f},"
              f"{media},"
              f"{entry['before']},"
              f"{entry['after']},"
              f"{entry['timescale']},"
              f"{entry['mPTSOffsetSec']},"
              f"{seg_duration},"
              f"{entry['log_duration']},"
              f"{entry['seg_num']},"
              f"{entry['profile']},"
              f"{entry['url']}")
