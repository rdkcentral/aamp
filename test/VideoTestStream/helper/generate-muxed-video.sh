#!/bin/bash

#####################################################
# Generate muxed A/V HLS with all languages from above
#####################################################

#add audio inputs
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
    AUDIO_INPUTS="$AUDIO_INPUTS -i ${AUDIO_PATH}/${LANG_639_2[$I]}/full_track.wav"
done

#add audio mapping
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
    AUDIO_INPUTS="$AUDIO_INPUTS -map $((I+1)):a"
    AUDIO_HLS_MAP="$AUDIO_HLS_MAP -map 0:a:$I"
done

#add audio tags
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
    AUDIO_INPUTS="$AUDIO_INPUTS -metadata:s:a:$I language=${LANG_639_3[$I]} -metadata:s:a:$I title=\"${LANG_FULL_NAME[$I]}\""
    AUDIO_HLS_MAP="$AUDIO_HLS_MAP -metadata:s:a:$I language=${LANG_639_3[$I]} -metadata:s:a:$I title=\"${LANG_FULL_NAME[$I]}\""
done

for (( I=0; I<PROFILE_COUNT; I++ ))
do
    #combine video with audio streams
    ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}.mp4 $AUDIO_INPUTS -map 0:v -c:v $VIDEO_CODEC -c:a $AUDIO_CODEC overlay${HEIGHT[$I]}_mux.mp4

    #generate muxed HLS
    ffmpeg -hide_banner -y -i overlay${HEIGHT[$I]}_mux.mp4  -map 0:v $AUDIO_HLS_MAP -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease -c:v $VIDEO_CODEC -c:a $AUDIO_CODEC -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -hls_time ${VIDEO_SEGMENT_SEC} -hls_playlist_type vod  -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_filename $HLS_OUT/${HEIGHT[$I]}p_mux_%03d.ts $HLS_OUT/${HEIGHT[$I]}p_mux.m3u8

done
