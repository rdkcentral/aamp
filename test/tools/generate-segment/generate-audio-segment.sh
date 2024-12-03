#!/bin/sh -e

# AUDIO_CODEC=aac (mp4a.40.2)
# AUDIO_CODEC=eac3 (ec-3)
# AUDIO_CODEC=ac3 (ac-3)

AUDIO_SAMPLING_RATE=$1
AUDIO_CODEC=$2
BASE_MEDIA_DECODE_TIME=$3
DURATION=$4
TIMESCALE=$5
SEGMENT_NUMBER=$6
MEDIA_FNAME=$7
INIT_FNAME=$8
SOURCE_MEDIA=$9

# floating point math
DURATION_S=$(echo "$DURATION $TIMESCALE" | awk '{printf "%f", $1 / $2}')
 
ffmpeg -y \
    -i $SOURCE_MEDIA \
    -t $DURATION_S \
    -c:a $AUDIO_CODEC \
    -movie_timescale $TIMESCALE \
    -ar $AUDIO_SAMPLING_RATE \
    audio.mp4

# Check if ffmpeg encountered an error
if [ $? -ne 0 ]; then
    echo "ffmpeg failed for audio generation"
    exit 1
fi

# split into init header and media segment
ffmpeg -i audio.mp4 -c:a copy -f dash -init_seg_name $INIT_FNAME -media_seg_name $MEDIA_FNAME audio.mpd

if [ $? -ne 0 ]; then
    echo "ffmpeg failed while splitting audio into init header and media segment"
    exit 1
fi

# repair timescale in init header
python3 modifyContentMetaData.py $INIT_FNAME --timescale $TIMESCALE

if [ $AUDIO_SAMPLING_RATE -eq 0 ]; then
    echo "WARNING: unspecified AUDIO_SAMPLING_RATE; using default 44100!"
    AUDIO_SAMPLING_RATE=44100
fi

DUR_ADJ=$(echo "(($DURATION * $TIMESCALE) % $AUDIO_SAMPLING_RATE)" | bc)
if [ $DUR_ADJ -ne 0 ]; then
    echo "WARNING: incompatible TIMESCALE and AUDIO_SAMPLING_RATE"
    exit
fi

# populate base media decode time and segment number
python3 modifyContentMetaData.py $MEDIA_FNAME --baseMediaDecodeTime $BASE_MEDIA_DECODE_TIME --segmentNumber $SEGMENT_NUMBER --timescale $TIMESCALE --audioSamplingRate $AUDIO_SAMPLING_RATE
