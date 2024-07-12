#!/bin/bash

# "IFRAME" text overlay for iframe track
I=0

DIR=video/iframe4k
mkdir "$DIR"
FILE_OVERLAY=overlayiframe4k.mp4
ffmpeg -y -i temp/loop.mp4 -vf \
drawtext="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2",\
drawtext="fontfile=/path/to/font.ttf: text='IFRAME ${HEIGHT[$I]}p': fontcolor=white: fontsize=${FONTSIZE[$I]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2" \
-codec:a  copy -c:v $VIDEO_CODEC -g $((GOP_SIZE)) $FILE_OVERLAY




# override iframe fragments with the same ones and generate DASH manifest

ffmpeg -hide_banner -y -i overlayiframe4k.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:v $VIDEO_CODEC -profile:v main -crf 20 -sc_threshold 0 -r $IFRAME_RATE -g 1 -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_type fmp4 -hls_time $IFRAME_CADENCE_SEC -hls_playlist_type vod -hls_segment_filename "$DIR"/iframe_'%03d.m4s' -hls_fmp4_init_filename iframe_init.m4s -start_number 1 "$DIR"/iframe.m3u8

ffmpeg -hide_banner -y -i overlayiframe4k.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:v $VIDEO_CODEC -crf 20 -sc_threshold 0 -r $IFRAME_RATE -g 1 -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -seg_duration $((IFRAME_CADENCE_SEC)) -use_timeline 1 -use_template 1 -init_seg_name iframe_init.m4s -media_seg_name iframe_'$Number%03d$.m4s' -f dash "$DIR"/iframe.mpd








