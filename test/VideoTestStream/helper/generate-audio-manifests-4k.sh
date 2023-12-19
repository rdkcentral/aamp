#!/bin/bash

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
	REPRESENTATION=${LANG_639_2[$I]}
	DIR=video/"$REPRESENTATION"
	FILE=temp/"$REPRESENTATION"/aac.mp4
	RAW=temp/"$REPRESENTATION"/full_track.wav
	mkdir "$DIR"
	FOUT="$DIR"/SegmentTimeline4k.mpd
	if [ -f "$FOUT" ]; then
       echo "$FOUT exists"
	else
		ffmpeg -y -i "$RAW" -map 0:a -c:a aac -b:a 384k -ar 48000 -t $VIDEO_LENGTH_SEC "$FILE"
		echo generating DASH SegmentTimeline audio
		ffmpeg -y -i "$FILE" -seg_duration "$AUDIO_SEGMENT_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number$.$ext$' -f dash "$FOUT"

	fi
done
