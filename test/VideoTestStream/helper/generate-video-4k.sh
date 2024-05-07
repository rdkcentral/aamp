
generate_4k_video_data() {
   FILE_LOOP=temp/loop.mp4
   if [ -f "$FILE_LOOP" ]; then
   echo "$FILE_LOOP exists"
   else
   echo generate video data duration="$VIDEO_LENGTH_SEC"s
   mkdir temp/frame
   ffmpeg -i https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/loop/loop.mp4 temp/frame/frame-%03d.jpg
   ffmpeg -stream_loop -1 -i "temp/frame/frame-%03d.jpg" -t $VIDEO_LENGTH_SEC -r $FPS_4K $FILE_LOOP
   fi

   for (( I=4; I<PROFILE_COUNT; I++ ))
   do
   W=${WIDTH[$I]}
   H=${HEIGHT[$I]}
   SCALE=scale=w=$((W)):h=$((H)):force_original_aspect_ratio=decrease
   MEDIATIME_OVERLAY="fontfile=/path/to/font.ttf: text='%{pts\:hms}': fontcolor=white: fontsize=${FONTSIZE[0]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h-text_h)/2"
   REPRESENTATION="${HEIGHT[$I]}"
   GOP_4K="$((FPS_4K*SEGMENT_CADENCE_SEC))"
   BANDWIDTH="${KBPS[$I]}k"
   MAXRATE="${MAXKBPS[$I]}k"
   FILE=temp/"$REPRESENTATION".mp4
   DIR=video/"$REPRESENTATION"
   mkdir "$DIR"
   if [ -f "$FILE" ]; then
   echo "$FILE exists"
   else
   TEXT="$W"x"$H"
   RESOLUTION_OVERLAY="fontfile=/path/to/font.ttf: text='$TEXT': fontcolor=white: fontsize=${FONTSIZE[0]}: box=1: boxcolor=black: boxborderw=5: x=(w-text_w)/2: y=(h/2-text_h)/2"
   ffmpeg -i temp/loop.mp4 -map 0:v -vf "$SCALE",drawtext="$MEDIATIME_OVERLAY",drawtext="$RESOLUTION_OVERLAY" -c:v $VIDEO_CODEC "$FILE"
   fi
   FOUT=video/"$REPRESENTATION"/video4k.mpd
   if [ -f "$FOUT" ]; then
       echo "$FILE exists"
   else
       echo generating DASH SegmentTimeline video
       ffmpeg -hide_banner -y -i "$FILE" -c:v $VIDEO_CODEC  -profile:v main -crf 20 -sc_threshold 0 -g "$GOP_4K" -b:v "$BANDWIDTH" -maxrate "$MAXRATE" -bufsize 1200k -seg_duration "$VIDEO_SEGMENT_SEC" -use_timeline 1 -use_template 1 -init_seg_name init.m4s -media_seg_name '$Number$.$ext$' -f dash "$FOUT"
   fi
   done
}

generate_4k_video_data
