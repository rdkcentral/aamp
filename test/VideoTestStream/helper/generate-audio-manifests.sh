#!/bin/bash

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
	echo ${LANG_639_2[$I]}":"
	
	#generate HLS audio track
	if [ $RUN_HLS -eq 1 ]; then
		FOUT=$HLS_OUT/${LANG_639_2[$I]}.m3u8
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
    		else
			ffmpeg -hide_banner -y -i ${AUDIO_PATH}/${LANG_639_2[$I]}/full_track.wav -map 0:a -c:a $AUDIO_CODEC -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -hls_time $VIDEO_SEGMENT_SEC -hls_playlist_type vod -hls_segment_filename $HLS_OUT/${LANG_639_2[$I]}_%d.ts $FOUT
		fi
	fi

    	#generate DASH content with HLS manifest output
	if [ $RUN_HLS_MP4 -eq 1 ]; then
		FOUT=$DASH_OUT/${LANG_639_2[$I]}.m3u8
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
		else
			ffmpeg -hide_banner -y -i ${AUDIO_PATH}/${LANG_639_2[$I]}/full_track.wav -map 0:a -c:a $AUDIO_CODEC -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -hls_segment_type fmp4  -hls_time $VIDEO_SEGMENT_SEC -hls_playlist_type vod -hls_segment_filename $DASH_OUT/${LANG_639_2[$I]}_'%03d.mp3' -hls_fmp4_init_filename ${LANG_639_2[$I]}_init.m4s -start_number 1  $FOUT
		fi
	fi

    	# override DASH audio with the same content and generate DASH manifest
	if [ $RUN_DASH -eq 1 ]; then
		FOUT=$DASH_OUT/${LANG_639_2[$I]}.mpd
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
		else
			ffmpeg -hide_banner -y -i ${AUDIO_PATH}/${LANG_639_2[$I]}/full_track.wav -map 0:a -c:a $AUDIO_CODEC -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC  -seg_duration $((VIDEO_SEGMENT_SEC)) -use_timeline 1 -use_template 1 -init_seg_name ${LANG_639_2[$I]}_init.m4s -media_seg_name ${LANG_639_2[$I]}_'$Number%03d$.mp3'  -f dash  $FOUT
		fi
	fi
done
