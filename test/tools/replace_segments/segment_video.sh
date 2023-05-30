#!/bin/sh
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

    echo "Input file:${1}  Duration:${2}  bit rate:${3}  Codec:${4}  PTS: ${5}  Dimension: ${6}  fps: ${7}"

    if [ $# -lt 5 ]; then exit 1; fi

    dir="${MPEG_WORKDIR:-tmp}"
    mkdir -p $dir
    rm $dir/chunk-stream* $dir/init-stream* 2>/dev/null

    if [ "${7}" = "1" ]; then iframe="-g 1"; fi

    ffmpeg -hide_banner -an -sn -t ${2} -stream_loop -1 -i ${1} \
        -s ${6} -r ${7:-25} -b:v ${3}k -codec:v ${4:-h264} $iframe -force_key_frames ${8:-expr:gte(t,n_forced*1)} \
        -vf "drawtext=fontsize=80: r=25: x=(w-tw)/2: y=h-(2*lh): fontcolor=white: box=1: borderw=10: boxcolor=0x00000099: text='${6} ${3}k %{pts\:flt\:0}'" \
        -f dash -seg_duration 6 -use_template 1 -use_timeline 0 -hls_playlist 1 $dir/convert_video.mpd

    exit $?
