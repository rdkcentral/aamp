#!/bin/bash
   
FILE_LOOP=loop.mp4

if [ -f "$FILE_LOOP" ]
then
	echo "$FILE_LOOP exists"
elif [ "$IMAGE" == 1 ]
then
   	echo generate video with image="$IMG_NAME" for duration="$VIDEO_LENGTH_SEC"s
	ffmpeg -loop 1 -i $IMG_NAME -c:v $VIDEO_CODEC -t $VIDEO_LENGTH_SEC -pix_fmt yuv420p -vf scale=1920:1080 -r $FPS $FILE_LOOP
else
   	echo generate video data with loop for duration="$VIDEO_LENGTH_SEC"
	mkdir temp/frames                                                                                            
	ffmpeg -i https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/loop/loop.mp4 temp/frames/frame-%03d.jpg
	ffmpeg -stream_loop -1 -i "temp/frames/frame-%03d.jpg" -t $VIDEO_LENGTH_SEC -r $FPS $FILE_LOOP
fi

echo "checking"
for (( I=0; I<PROFILE_COUNT; I++ ))
do
	FONTSIZE=48*${HEIGHT[$I]}/1080
	FILE=temp/"${HEIGHT[$I]}".mp4

	if [ -f "$FILE" ]; then
		echo "$FILE exists"
	else
		TEXT="${HEIGHT[$I]}"x"${WIDTH[$I]}"
		MEDIATIME_OVERLAY="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=$FONTSIZE: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2"
		RESOLUTION_OVERLAY="fontfile=/path/to/font.ttf: text='$TEXT': fontcolor=white: fontsize=$FONTSIZE: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2"
		TEXTTRACK="NONE"
		if [ $GEN_TTML -eq 1 ]; then
			TEXTTRACK="TTML"
		elif [ $GEN_WEBVTT -eq 1 ]; then 
			TEXTTRACK="WebVTT"
		fi

		VIDEO_TEXT=Video"\ \:\ $VIDEO_CODEC"
 		VIDEO_OVERLAY="fontfile=/path/to/font.ttf: text='$VIDEO_TEXT': fontcolor=white: fontsize=$FONTSIZE: box=1: boxcolor=black: boxborderw=5: x=1: y=1"
		AUDIO_TEXT=Audio"\ \:\ $AUDIO_CODEC"
		AUDIO_OVERLAY="fontfile=/path/to/font.ttf: text='$AUDIO_TEXT': fontcolor=white: fontsize=$FONTSIZE: box=1: boxcolor=black: boxborderw=5: x=1: y=((${HEIGHT[$I]}/360*20))"
		TEXT_TEXT=Text"\   \:\ $TEXTTRACK"
 		TEXT_OVERLAY="fontfile=/path/to/font.ttf: text='$TEXT_TEXT': fontcolor=white: fontsize=$FONTSIZE: box=1: boxcolor=black: boxborderw=5: x=1: y=((${HEIGHT[$I]}/360*40))"
	
		ffmpeg -i loop.mp4 -map 0:v -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease,drawtext="$MEDIATIME_OVERLAY",drawtext="$RESOLUTION_OVERLAY",drawtext="$VIDEO_OVERLAY",drawtext="$AUDIO_OVERLAY",drawtext="$TEXT_OVERLAY" -c:v $VIDEO_CODEC "$FILE"
	fi
   
	# Generate DASH
	if [ $RUN_DASH -eq 1 ]; then
		FOUT=$DASH_OUT/"${HEIGHT[$I]}"p.mpd
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
		else
			echo Generating DASH SegmentTimeline video
			ffmpeg -hide_banner -y -i "$FILE" -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease  -c:v $VIDEO_CODEC -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -seg_duration $((VIDEO_SEGMENT_SEC)) -use_timeline 1 -use_template 1 -init_seg_name $DASH_OUT/${HEIGHT[$I]}p_init.m4s -media_seg_name $DASH_OUT/${HEIGHT[$I]}p_'$Number%03d$.m4s'  -f dash ${HEIGHT[$I]}p.mpd
		fi
	fi

	# Generate fragmented HLS
	if [ $RUN_HLS -eq 1 ]; then
		FOUT=$HLS_OUT/${HEIGHT[$I]}p.m3u8
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
		else
			echo Generating Fragmented HLS SegmentTimeline video
			ffmpeg -hide_banner -y -i "$FILE" -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease -c:v $VIDEO_CODEC -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -hls_time ${VIDEO_SEGMENT_SEC} -hls_playlist_type vod  -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_filename $HLS_OUT/${HEIGHT[$I]}p_%03d.ts $HLS_OUT/${HEIGHT[$I]}p.m3u8 
		fi
	fi
	
	# Generate HLS MP4
	if [ $RUN_HLS_MP4 -eq 1 ]; then
		FOUT=$DASH_OUT/${HEIGHT[$I]}p.m3u8
		if [ -f "$FOUT" ]; then
			echo "$FOUT exists"
		else
			echo generating HLS Mp4 SegmentTimeline video
			ffmpeg -hide_banner -y -i "$FILE" -map 0:v  -vf scale=w=${WIDTH[$I]}:h=${HEIGHT[$I]}:force_original_aspect_ratio=decrease  -c:v $VIDEO_CODEC -profile:v main -crf 20 -sc_threshold 0 -g $((FPS*VIDEO_SEGMENT_SEC)) -b:v ${KBPS[$I]}k -maxrate ${MAXKBPS[$I]}k -bufsize 1200k -hls_segment_type fmp4  -hls_time $VIDEO_SEGMENT_SEC -hls_playlist_type vod -hls_segment_filename $DASH_OUT/${HEIGHT[$I]}p_'%03d.m4s' -hls_fmp4_init_filename ${HEIGHT[$I]}p_init.m4s -start_number 1  $DASH_OUT/${HEIGHT[$I]}p.m3u8
		fi
	fi
done
