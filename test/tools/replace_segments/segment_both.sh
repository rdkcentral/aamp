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
    echo "Input file:${1}  Durations:${2}  PTS: ${3}  video bit rate:${4} Codec: ${5} Dimension: ${6}  audio bit rate: ${7} Codec: ${8}  fps: ${9}"

    if [ $# -lt 4 ]; then exit 1; fi

    dir="${MPEG_WORKDIR:-tmp}"
    mkdir -p $dir
    rm $dir/chunk-stream* $dir/init-stream* 2>/dev/null

    dur="${2##*,}"

    if [ "${4}" != "" ]; then
        if [ "${9}" = "1" ]; then iframe="-g 1"; fi

        do_video="-r ${9:-25} $iframe -force_key_frames "${2}" -b:v ${4}k -codec:v ${5:-h264}"
        adj_video=",setpts=PTS+${3}/TB"
    else
        do_video="-vn"
    fi

    if [ "${7}" != "" ]; then
        do_audio="-codec:a ${8:-aac} -b:a ${7}k -ac 2"
        adj_audio="-af asetpts=PTS+${3}/TB"
    else
        do_audio="-an"
    fi

    ffmpeg -hide_banner -sn -t $dur -stream_loop -1 -i ${1} \
        -s ${6} $do_video $do_audio \
        -vf "drawtext=fontsize=80: r=25: x=(w-tw)/2: y=h-(2*lh): fontcolor=white: box=1: borderw=10: boxcolor=0x00000099: text='${6} ${4}k %{pts\:flt\:0}'"$adj_video \
        $adj_audio -f segment -segment_times "${2}" -break_non_keyframes 1 -segment_list_type m3u8 -segment_list $dir/transport.m3u8 $dir/chunk-stream0%04d.ts

    exit $?
