#!/bin/bash

# convert single image to video; "t" is target duration in seconds
FILE_LOOP=loop.mp4
if [ -f "$FILE_LOOP" ]; then
	echo "$FILE_LOOP exists"
else
	ffmpeg -loop 1 -i testpat.jpg -c:v libx264 -t $VIDEO_LENGTH_SEC -pix_fmt yuv420p -vf scale=1920:1080 -r $FPS $FILE_LOOP
fi

# generate profiles
for I in {0..3}
do
# add resolution and current-time as overlay
FILE_OVERLAY=overlay${HEIGHT[$I]}.mp4
if [ -f "$FILE_OVERLAY" ]; then
	echo "$FILE_OVERLAY exists"
else
ffmpeg -i loop.mp4 -vf \
drawtext="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2",\
drawtext="fontfile=/path/to/font.ttf: text='${HEIGHT[$I]}p': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2" \
-codec:a copy $FILE_OVERLAY
fi

# Generate DASH content with HLS manifest
ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease  -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_type fmp4  -hls_time $VIDEO_SEGMENT_SEC -hls_playlist_type vod -hls_segment_filename $DASH_OUT/${HEIGHT[$I]}p_'%03d.m4s' -hls_fmp4_init_filename ${HEIGHT[$I]}p_init.m4s -start_number 1  $DASH_OUT/${HEIGHT[$I]}p.m3u8

# Override DASH video with the same content and generate DASH manifest
ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease  -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -seg_duration $((VIDEO_SEGMENT_SEC)) -use_timeline 1 -use_template 1 -init_seg_name $DASH_OUT/${HEIGHT[$I]}p_init.m4s -media_seg_name $DASH_OUT/${HEIGHT[$I]}p_'$Number%03d$.m4s'  -f dash ${HEIGHT[$I]}p.mpd

# Generate fragmented HLS
ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -hls_time ${VIDEO_SEGMENT_SEC} -hls_playlist_type vod  -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_filename $HLS_OUT/${HEIGHT[$I]}p_%03d.ts $HLS_OUT/${HEIGHT[$I]}p.m3u8
done
​
