#!/bin/bash

# "IFRAME" text overlay for iframe track
I=0
FILE_OVERLAY=overlayiframe.mp4
ffmpeg -y -i loop.mp4 -vf \
drawtext="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2",\
drawtext="fontfile=/path/to/font.ttf: text='IFRAME ${HEIGHT[$I]}p': fontcolor=white: fontsize=48: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2" \
-codec:a copy -g $((GOP_SIZE)) $FILE_OVERLAY
# single segment duration: IFRAME_CADENCE_SEC
# single segment FPS: 1/IFRAME_CADENCE_SEC
# what gives one I-Frame per segment and nothing else
# Generate iframe fragments and playlist for hls
if [ $RUN_HLS -eq 1 ]; then
	ffmpeg -hide_banner -y -i overlayiframe.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -r $IFRAME_RATE -g 1 -hls_time $IFRAME_CADENCE_SEC -hls_playlist_type vod -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_filename $HLS_OUT/iframe_%03d.ts $HLS_OUT/iframe.m3u8
fi

# Generate iframe fragments using hls fmp4 extension with HLS manifest
if [ $RUN_DASH -eq 1 ]; then
	ffmpeg -hide_banner -y -i overlayiframe.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -r $IFRAME_RATE -g 1 -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_type fmp4 -hls_time $IFRAME_CADENCE_SEC -hls_playlist_type vod -hls_segment_filename $DASH_OUT/iframe_'%03d.m4s' -hls_fmp4_init_filename iframe_init.m4s -start_number 1 $DASH_OUT/iframe.m3u8
fi

# override iframe fragments with the same ones and generate DASH manifest

if [ $RUN_DASH -eq 1 ]; then
	ffmpeg -hide_banner -y -i overlayiframe.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,select='eq(pict_type\,PICT_TYPE_I)' -c:v h264 -profile:v main -crf 20 -sc_threshold 0 -r $IFRAME_RATE -g 1 -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -seg_duration $((IFRAME_CADENCE_SEC)) -use_timeline 1 -use_template 1 -init_seg_name $DASH_OUT/iframe_init.m4s -media_seg_name $DASH_OUT/iframe_'$Number%03d$.m4s' -f dash iframe.mpd
fi
