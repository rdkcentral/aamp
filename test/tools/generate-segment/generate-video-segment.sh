#!/bin/sh -e

# VIDEO_CODEC=h264 (avc1.4d4028)
# VIDEO_CODEC=hevc (hvc1.1.6.L93.90)

WIDTH=$1
HEIGHT=$2
FPS=$3
VIDEO_CODEC=$4
BASE_MEDIA_DECODE_TIME=$5
DURATION=$6
TIMESCALE=$7
SEGMENT_NUMBER=$8
MEDIA_FNAME=$9
INIT_FNAME=${10}
SOURCE_MEDIA=${11}
GENERATE_LL_SEGMENTS=${12}

# floating point math
DURATION_S=$(echo "$DURATION $TIMESCALE" | awk '{printf "%f", $1 / $2}')
START_TIME_S=$(echo "scale=3; $BASE_MEDIA_DECODE_TIME/$TIMESCALE" | bc)

#For iframes we get fractional values, truncate for display
FPS_DISPLAY=$(echo "scale=2; $FPS" | bc)
# scale text proportional to resolution
FONTSIZE="48*$HEIGHT/1080"
SCALE=scale=w=$((WIDTH)):h=$((HEIGHT)):force_original_aspect_ratio=decrease


MEDIATIME_OVERLAY="fontfile=/path/to/font.ttf: \
    text='PTS%{eif\:t+$START_TIME_S\:d\:2}.%{eif\:(mod((t+$START_TIME_S)*1000\,1000))\:d\:3} SEG$SEGMENT_NUMBER-%{frame_num}': \
    fontcolor=white: fontsize=$FONTSIZE: \
    box=1: boxcolor=black: boxborderw=10: x=(w-text_w)/2: y=(h-text_h)/2"

TEXT="$WIDTH"x"$HEIGHT"-"$FPS_DISPLAY"fps-"$VIDEO_CODEC"

RESOLUTION_OVERLAY="fontfile=/path/to/font.ttf: \
    text='$TEXT': \
    fontcolor=white: \
    fontsize=$FONTSIZE: \
    box=1: boxcolor=black: boxborderw=10: x=(w-text_w)/2: y=(h/2-text_h)/2"

# low-latency options
LL_OPTIONS=""
if [ "$GENERATE_LL_SEGMENTS" = "generate_chunked_segments" ]; then
LL_OPTIONS="\
-frag_type duration \
-frag_duration 0.48 \
-ldash 1 \
-utc_timing_url 'https://time.akamai.com?iso&amp;ms' \
-write_prft 1 \
"
fi


ffmpeg -y \
    -loop 1 \
    -i $SOURCE_MEDIA \
    -vf "$SCALE,fps=$FPS,drawtext=$MEDIATIME_OVERLAY,drawtext=$RESOLUTION_OVERLAY" \
    -t $DURATION_S \
    -c:v $VIDEO_CODEC \
    -movie_timescale $TIMESCALE \
    -video_track_timescale $TIMESCALE \
    -metadata segment_number="$SEGMENT_NUMBER" \
    video.mp4

# split into init header and media segment
ffmpeg -i video.mp4 -c:v copy -copytb 1 $LL_OPTIONS -f dash -init_seg_name $INIT_FNAME -media_seg_name $MEDIA_FNAME video.mpd

if [ $? -ne 0 ]; then
    echo "ffmpeg failed while splitting video into init header and media segment"
    exit 1
fi
# repair timescale in init header. The moov timescale is reset to default (1000) when extracting the init segment above.
# Probably not required but since its been set in the ffmpeg command above, maintain it.
python3 modifyContentMetaData.py $INIT_FNAME --timescale $TIMESCALE

# populate base media decode time and segment number
python3 modifyContentMetaData.py $MEDIA_FNAME --baseMediaDecodeTime $BASE_MEDIA_DECODE_TIME --segmentNumber $SEGMENT_NUMBER

