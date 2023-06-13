#!/bin/bash

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
	REPRESENTATION=${LANG_639_2[$I]}
	DIR=video/"$REPRESENTATION"
	FILE=temp/"$REPRESENTATION"/aac.mp4
	RAW=temp/"$REPRESENTATION"/full_track.wav
	mkdir "$DIR"
	FOUT="$DIR"/SegmentTimeline.mpd
	if [ -f "$FOUT" ]; then
       echo "$FOUT exists"
	else
		ffmpeg -y -i "$RAW" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC "$FILE"
		echo generating DASH SegmentTimeline audio
		ffmpeg -y -i "$FILE" -seg_duration "$AUDIO_SEGMENT_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$-$Time$.$ext$' -f dash "$FOUT"
		echo generating DASH SegmentTemplate audio
		ffmpeg -y -i "$FILE" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -seg_duration "$AUDIO_SEGMENT_SEC" -use_timeline 0 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number%05d$.$ext$' -f dash "$DIR"/SegmentTemplate.mpd
		echo generating DASH SegmentList audio
		ffmpeg -y -i "$FILE" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -seg_duration "$AUDIO_SEGMENT_SEC" -use_timeline 0 -use_template 0 -init_seg_name init.m4s -media_seg_name 'T_$Number%05d$-$Time$.$ext$' -f dash "$DIR"/SegmentList.mpd
		echo generating DASH SegmentBase audio
		ffmpeg -y -i "$FILE" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -seg_duration "$AUDIO_SEGMENT_SEC" -single_file 1 -single_file_name base.mp4  -f dash "$DIR"/SegmentBase.mpd
		cd "$DIR" || exit
		echo generating fragmented mp4 HLS audio
		ffmpeg -y -i ../../"$FILE" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -hls_time "$AUDIO_SEGMENT_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.mp4' -hls_segment_type fmp4 -hls_fmp4_init_filename init.m4s -start_number 1 fragmented_mp4.m3u8
		echo generating HLS/ts audio
		ffmpeg -y -i ../../"$FILE" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC -hls_time "$AUDIO_SEGMENT_SEC" -hls_playlist_type vod -hls_segment_filename '%05d.ts' -start_number 1 hls_ts.m3u8
		cd ../..
		# workaround: ffmpeg generates first audio segment with unexpected filename
		cp video/"$REPRESENTATION"/00001--1024.m4s video/"$REPRESENTATION"/00001-0.m4s
	fi
done
