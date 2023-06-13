#!/bin/bash
# https://www.shellcheck.net/
# shellcheck disable=SC2016

source helper/install-dependencies.sh
source helper/config-4k.sh

generate_iframe_track() {
   echo generating IFRAME track
   I=0
   W=${WIDTH[$I]}
   H=${HEIGHT[$I]}
   REPRESENTATION="iframe"
   RATE=$((1/IFRAME_CADENCE_SEC))
   GOP="$((FPS*IFRAME_CADENCE_SEC))"
   BANDWIDTH="${KBPS[$I]}k"
   MAXRATE="${MAXKBPS[$I]}k"
   FILE=temp/iframe.mp4
   DIR="video/iframe"
   mkdir "$DIR"
   if [ -f "$FILE" ]; then
      echo "$FILE exists"
   else
      SCALE=scale=w=$((W)):h=$((H)):force_original_aspect_ratio=decrease
      MEDIATIME_OVERLAY="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2"
      RESOLUTION_OVERLAY="fontfile=/path/to/font.ttf: text='iframe': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2"
      
      # pass#1 - scale, adjust GOP, add iframe text overlay
      ffmpeg -y -i temp/loop.mp4 -vf "$SCALE",drawtext="$MEDIATIME_OVERLAY",drawtext="$RESOLUTION_OVERLAY" -codec:a copy -g $((FPS*IFRAME_CADENCE_SEC)) temp/iframe-prep.mp4
      # pass#2 - cut iframes
      ffmpeg -y -i temp/iframe-prep.mp4 -vf select='eq(pict_type\,PICT_TYPE_I)' -map 0:v -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -r $RATE -g 1 -b:v "$BANDWIDTH" -maxrate "$MAXRATE" -bufsize 1200k temp/iframe.mp4
   fi
   
   FOUT="$DIR"/SegmentTimeline.mpd
   if [ -f "$FOUT" ]; then
       echo "$FOUT exists"
   else
       echo generating DASH SegmentTimeline iframe
       ffmpeg -y -i "$FILE" -seg_duration "$IFRAME_CADENCE_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$-$Time$.$ext$' -f dash "$FOUT"
       echo generating DASH SegmentTemplate iframe
       ffmpeg -y -i "$FILE" -seg_duration "$IFRAME_CADENCE_SEC" -use_timeline 0 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$.$ext$' -f dash "$DIR"/SegmentTemplate.mpd
       echo generating DASH SegmentList iframe
       ffmpeg -y -i "$FILE" -seg_duration "$IFRAME_CADENCE_SEC" -use_timeline 0 -use_template 0 -init_seg_name init.m4s -media_seg_name 'T_$Number%05d$-$Time$.$ext$' -f dash "$DIR"/SegmentList.mpd
       echo generating DASH SegmentBase iframe
       ffmpeg -y -i "$FILE" -seg_duration "$IFRAME_CADENCE_SEC" -single_file 1 -single_file_name base.mp4  -f dash "$DIR"/SegmentBase.mpd
       echo generating fragmented mp4 HLS iframe
       cd "$DIR" || exit
       ffmpeg -hide_banner -y -i ../../"$FILE" -hls_time "$IFRAME_CADENCE_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.m4s' -hls_segment_type fmp4 -hls_fmp4_init_filename init.m4s -start_number 1 fragmented_mp4.m3u8
       echo generating HLS/ts iframe
       ffmpeg -hide_banner -y -i ../../"$FILE" -hls_time "$IFRAME_CADENCE_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.ts' -start_number 1 hls_ts.m3u8
       cd ../..
   fi
}

generate_muxed_content() {
    echo generating muxed content
    for (( I=0; I<LANGUAGE_COUNT; I++ ))
    do
        AUDIO_INPUTS="$AUDIO_INPUTS -i temp/${LANG_639_2[$I]}/full_track.wav"
    done
    for (( I=0; I<LANGUAGE_COUNT; I++ ))
    do
        AUDIO_INPUTS="$AUDIO_INPUTS -map $((I+1)):a"
    done
    for (( I=0; I<LANGUAGE_COUNT; I++ ))
    do
        FULLNAME=${LANG_NAME[$I]}
        LANG3=${LANG_639_3[$I]}
        AUDIO_INPUTS="$AUDIO_INPUTS -metadata:s:a:$I language=$LANG3 -metadata:s:a:${I} title='"${FULLNAME}"'"
    done
    
    ffmpeg -y -i temp/2160.mp4 ${AUDIO_INPUTS} -map 0:v video/muxed.mp4
    ffmpeg -y -i video/muxed.mp4 -hls_segment_filename 'muxed-%05d.ts' -start_number 1 Muxed.m3u8
}

generate_video_data() {
   FILE_LOOP=temp/loop.mp4
   if [ -f "$FILE_LOOP" ]; then
   echo "$FILE_LOOP exists"
   else
   echo generate video data duration="$VIDEO_LENGTH_SEC"s
   
   # static test pattern
   # ffmpeg -loop 1 -i testpat.jpg -t $VIDEO_LENGTH_SEC -r $FPS $FILE_LOOP
   # animated test pattern
   # ffmpeg -i loop.mp4 -t $VIDEO_LENGTH_SEC -r $FPS $FILE_LOOP
   ffmpeg -i https://cpetestutility.stb.r53.xcal.tv/aamptest/streams/loop/loop.mp4 -t $VIDEO_LENGTH_SEC -r $FPS $FILE_LOOP
   
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
   ffmpeg -i temp/loop.mp4 -map 0:v -vf "$SCALE",drawtext="$MEDIATIME_OVERLAY",drawtext="$RESOLUTION_OVERLAY" -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g "$GOP" -b:v "$BANDWIDTH" -maxrate "$MAXRATE" -bufsize 1200k -codec:a copy "$FILE"
   fi
   
   FOUT="$DIR"/SegmentTimeline.mpd
   if [ -f "$FOUT" ]; then
       echo "$FILE exists"
   else
       echo generating DASH SegmentTimeline video
       ffmpeg -hide_banner -y -i "$FILE" -seg_duration "$VIDEO_SEGMENT_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$-$Time$.$ext$'  -f dash "$DIR"/SegmentTimeline.mpd
       echo generating DASH SegmentTemplate video
       ffmpeg -hide_banner -y -i "$FILE" -seg_duration "$VIDEO_SEGMENT_SEC" -use_timeline 0 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$.$ext$'  -f dash "$DIR"/SegmentTemplate.mpd
       echo generating DASH SegmentList video
       ffmpeg -hide_banner -y -i "$FILE" -seg_duration "$VIDEO_SEGMENT_SEC" -use_timeline 0 -use_template 0 -init_seg_name init.m4s -media_seg_name 'T_$Number%05d$-$Time$.$ext$'  -f dash "$DIR"/SegmentList.mpd
       echo generating DASH SegmentBase video
       ffmpeg -hide_banner -y -i "$FILE" -seg_duration "$VIDEO_SEGMENT_SEC" -single_file 1 -single_file_name base.mp4  -f dash "$DIR"/SegmentBase.mpd
       cd "$DIR" || exit
       echo generating fragmented mp4 HLS video
       ffmpeg -hide_banner -y -i ../../"$FILE" -hls_time "$VIDEO_SEGMENT_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.m4s' -hls_segment_type fmp4 -hls_fmp4_init_filename init.m4s -start_number 1 fragmented_mp4.m3u8
       echo generating HLS/ts video
       ffmpeg -hide_banner -y -i ../../"$FILE" -hls_time "$VIDEO_SEGMENT_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.ts' -start_number 1 hls_ts.m3u8
       cd ../..
   fi
   done
}

source helper/stitch-manifest.sh

mkdir temp
mkdir video

# read -p "press any key to generate video data"
generate_video_data

# read -p "press any key to generate video tracks"
generate_video_tracks

# read -p "press any key to generate iframe track"
generate_iframe_track

# read -p "press any key to generate audio tracks"
source helper/generate-audio-data.sh
source helper/generate-audio-manifests-4k.sh

# read -p "press any key to generate muxed content"
generate_muxed_content

# read -p "press any key to stitch manifests"
stitch_manifests SegmentTemplate.mpd
stitch_manifests SegmentTimeline.mpd
stitch_manifests SegmentBase.mpd
stitch_manifests SegmentList.mpd
