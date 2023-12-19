#!/bin/bash
# https://www.shellcheck.net/
# shellcheck disable=SC2016

source helper/install-dependencies.sh
source helper/config-4k.sh
source helper/stitch-manifest.sh

generate_video_data() {
   FILE_LOOP=temp/loop.mp4
   if [ -f "$FILE_LOOP" ]; then
   echo "$FILE_LOOP exists"
   else
   echo generate video data duration="$VIDEO_LENGTH_SEC"s
   mkdir temp/frame
   ffmpeg -i https://cpetestutility.stb.r53.xcal.tv/aamptest/streams/loop/loop.mp4 temp/frame/frame-%03d.jpg
   ffmpeg -stream_loop -1 -i "temp/frame/frame-%03d.jpg" -t $VIDEO_LENGTH_SEC -r $FPS $FILE_LOOP
   fi
}
generate_video_tracks() {
   for (( I=0; I<PROFILE_COUNT; I++ ))
   do
   W=${WIDTH[$I]}
   H=${HEIGHT[$I]}
   SCALE=scale=w=$((W)):h=$((H)):force_original_aspect_ratio=decrease
   MEDIATIME_OVERLAY="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2"
   REPRESENTATION="${HEIGHT[$I]}"
   GOP="$((FPS*SEGMENT_CADENCE_SEC))"
   BANDWIDTH="${KBPS[$I]}k"
   MAXRATE="${MAXKBPS[$I]}k"
   FILE=temp/"$REPRESENTATION".mp4
   DIR=video/"$REPRESENTATION"
   mkdir "$DIR"
   if [ -f "$FILE" ]; then
   echo "$FILE exists"
   else
   TEXT="$W"x"$H"
   RESOLUTION_OVERLAY="fontfile=/path/to/font.ttf: text='$TEXT': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2"
   ffmpeg -i temp/loop.mp4 -map 0:v -vf "$SCALE",drawtext="$MEDIATIME_OVERLAY",drawtext="$RESOLUTION_OVERLAY" -c:v $VCODEC "$FILE"
   fi
   FOUT="$DIR"/SegmentTimeline4k.mpd
   if [ -f "$FOUT" ]; then
       echo "$FILE exists"
   else
       echo generating DASH SegmentTimeline video
       ffmpeg -hide_banner -y -i "$FILE" -c:v $VCODEC  -profile:v main -crf 20 -sc_threshold 0 -g "$GOP" -b:v "$BANDWIDTH" -maxrate "$MAXRATE" -bufsize 1200k -seg_duration "$VIDEO_SEGMENT_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number$.$ext$'  -f dash "$DIR"/SegmentTimeline4k.mpd
   fi
   done
}

mkdir temp
mkdir video

generate_video_data
generate_video_tracks
source helper/generate-audio-data.sh
source helper/generate-audio-manifests-4k.sh
stitch_manifests SegmentTimeline4k.mpd

