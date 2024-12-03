#!/bin/sh -x
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

    echo "Input file:${1}  Duration:${2}  bit rate:${3}  Codec:${4}  PTS: ${5}  Dimension: ${6}  fps: ${7} iframe {8} text: ${9} Total_MPD_Duration: ${10} Skip_Donor_Vid: ${11}"

    if [ $# -lt 5 ]; then exit 1; fi

    dir="${MPEG_WORKDIR:-tmp}"
    mkdir -p $dir
    rm $dir/chunk-stream* $dir/init-stream* $dir/*.mp4 2>/dev/null

    if [ "${7}" = "1" ]; then iframe="-g 1"; fi

    if [ "${12}" = "yes" ]
    then
        ffmpeg -hide_banner -loglevel panic -an -sn -ss ${11} -i ${1} \
		-s ${6} -r ${7:-25} -b:v ${3}k -codec:v ${4:-h264} $iframe -force_key_frames ${8:-expr:gte(t,n_forced*1)} \
		-vf "drawtext=fontsize=30: box=1: x=(w-text_w)/2:y=(h-text_h)/2: text='${9} %{pts\:flt\:0}'" \
		-f dash -seg_duration 6 -use_template 1 -use_timeline 0 -hls_playlist 1 $dir/convert_video1.mpd
	ffmpeg -hide_banner -loglevel panic -an -sn -t ${13} -stream_loop -1 -i ${1} \
		-s ${6} -r ${7:-25} -b:v ${3}k -codec:v ${4:-h264} $iframe -force_key_frames ${8:-expr:gte(t,n_forced*1)} \
		-vf "drawtext=fontsize=30: box=1: x=(w-text_w)/2:y=(h-text_h)/2: text='${9} %{pts\:flt\:0}'" \
		-f dash -seg_duration 6 -use_template 1 -use_timeline 0 -hls_playlist 1 $dir/convert_video2.mpd
	ffmpeg -i $dir/convert_video1.mpd -i $dir/convert_video2.mpd -c copy -map 0 -map 1 -f dash $dir/convert_video.mpd
	rm -f $dir/convert_video1.mpd $dir/convert_video2.mpd
	exit $?
    else
        ffmpeg -hide_banner -loglevel panic -an -sn -ss ${11} -t ${2} -i ${1} \
		-s ${6} -r ${7:-25} -b:v ${3}k -codec:v ${4:-h264} $iframe -force_key_frames ${8:-expr:gte(t,n_forced*1)} \
		-vf "drawtext=fontsize=30: box=1: x=(w-text_w)/2:y=(h-text_h)/2: text='${9} %{pts\:flt\:0} | %{pts\:gmtime\:${14}\:%M\\\\\:%S}'" \
		-f dash -seg_duration 6 -use_template 1 -use_timeline 0 -hls_playlist 1 $dir/convert_video.mpd
	exit $?
    fi 
