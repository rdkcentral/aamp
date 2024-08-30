#!/usr/bin/python3
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
from datetime import datetime

counts = {}
timescales = {}


def init():
    for med in ['audio', 'video', 'subtitle']:
        counts.setdefault(med, [])
        timescales.setdefault(med, 1)


init()
for line in fileinput.input(errors="ignore"):

    m = re.search(r'mManifestUrl:(.*)', line)
    if m:
        init()
        tune = m.group(1)
        for media in ['audio', 'video', 'subtitle']:
            counts[media].append({'msg': f'Tune {tune}'})

    # video timeScale 90000 mPTSOffsetSec - 15221.428622
    # get the timescale from the log line that should preceed RestampPts
    m = re.search(r'(?:ProcessFragmentChunk|ProcessAndInjectFragment).*(audio|video|subtitle) timeScale (\d+)', line)
    if m:
        media = m.group(1)
        ts = int(m.group(2))
        timescales[media] = ts

    m = re.search(r'RestampPts.*before (\d+) after (\d+) duration (\d+) (.*)',line)
    if m:
        before = int(m.group(1))
        after = int(m.group(2))
        log_duration = int(m.group(3))
        url = m.group(4)
        media = 'video'
        if "audio" in url:
            media = 'audio'
        elif ("subtitle" in url) or ("text" in url):
            media = 'subtitle'
        counts.setdefault(media, [])
        counts[media].append({'before': before, 'after': after, 'timescale': timescales[media], 'log_duration': log_duration,'url':url})

for media in ['video', 'audio', 'subtitle']:
    print(media)
    # excel column headings
    print('Original PTS in seg (from log),', end="")
    print('PTS after restamp (from log),', end="")
    print('timescale (from log),', end="")
    print('PTS Offset (calculated),', end="")
    print('PTS(n) - PTS(n+1) I.E duration (calculated),', end="")
    print('duration (from log),', end="")
    print('url (from log)')

    stream = counts[media]
    for idx, entry in enumerate(stream):
        if 'msg' in entry:
            print(entry['msg'])
            previous = 0
            continue

        # duration of this segment is pts(n+1) - pts(n)
        if (idx+1) < len(stream) and 'after' in stream[idx+1]:
            next_entry = stream[idx+1]
            seg_duration = next_entry['after']*entry['timescale']/next_entry['timescale'] - entry['after']
        else:
            seg_duration = 0
        offset_applied = entry['after'] - entry['before']
        print(f"{entry['before']},{entry['after']},{entry['timescale']},{offset_applied},{seg_duration},{entry['log_duration']},{entry['url']}")
