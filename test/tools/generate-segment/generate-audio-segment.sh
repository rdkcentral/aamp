#!/bin/sh

# AUDIO_CODEC=aac (mp4a.40.2)
# AUDIO_CODEC=eac3 (ec-3)
# AUDIO_CODEC=ac3 (ac-3)

AUDIO_CODEC=$1
BASE_MEDIA_DECODE_TIME=$2
DURATION=$3
TIMESCALE=$4
SEGMENT_NUMBER=$5
MEDIA_FNAME=$6
INIT_FNAME=$7
SOURCE_MEDIA=$8

# leverage bc for floating point math
DURATION_S=$(echo "scale=3; $DURATION/$TIMESCALE" | bc)

ffmpeg -y \
    -i $SOURCE_MEDIA \
    -t $DURATION_S \
    -c:a $AUDIO_CODEC \
	-movie_timescale $TIMESCALE \
    -video_track_timescale $TIMESCALE \
    audio.mp4


# split into init header and media segment
ffmpeg -i audio.mp4 -c:a copy -f dash -init_seg_name $INIT_FNAME -media_seg_name $MEDIA_FNAME audio.mpd

# repair timescale in init header
python3 modifyContentMetaData.py $INIT_FNAME --timescale $TIMESCALE

# populate base media decode time and segment number
python3 modifyContentMetaData.py $MEDIA_FNAME --baseMediaDecodeTime $BASE_MEDIA_DECODE_TIME --segmentNumber $SEGMENT_NUMBER
