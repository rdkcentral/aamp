#!/bin/sh
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
    echo "Input file:$1  Duration:$2"

    if [ $# -lt 2 ]; then exit 1; fi

    dir="${MPEG_WORKDIR:-tmp}"
    mkdir -p $dir
    rm $dir/chunk-stream* $dir/init-stream* 2>/dev/null

    ffmpeg -hide_banner -an -vn -t $2 -i $1 \
        -f dash -seg_duration 6 -use_template 1 -use_timeline 0 -hls_playlist 1 $dir/convert_data.mpd

    exit $?
