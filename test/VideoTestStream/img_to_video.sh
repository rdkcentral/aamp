#!/bin/bash

set -e

if [ "$#" -eq 1 ] && [ "$1" = "-h" ]; then
  echo "Usage: img_to_video.sh image output fps timescale start_pts duration video_codec text"
  echo ""
  echo "parameters:"  
  echo "image: the input image file name"
  echo "output: the output video file name"
  echo "fps: frame per second for the output video file"
  echo "timescale: tbn: the reciprocal of the timebase, example value: 60, 75, 90000"
  echo "start_pts: start time stamp in second, can be a floating number"
  echo "duration: video duration in second"
  echo "video_codec: codec to encode the video. exmaple value: h264"
  echo "text: text overlay. exmaple value: 'my video' "
  echo ""
  echo "Options:"
  echo "  -h    Display this help message."
  exit 0
fi

if [ $# -ne 8 ]; then
    echo "Usage: img_to_video.sh image output fps timescale start_pts duration video_codec text"
    exit 1
fi

image="$1"
output="$2"
fps="$3"
timescale="$4"
start_pts="$5"
duration="$6"
video_codec="$7"
text="$8"

echo "start ts offset is: $start_pts"
echo "codec is: $video_codec"

ffmpeg -loop 1 -y -i "$image"  -stream_loop -1 -i sfx.mp3 -c:v "$video_codec" -c:a copy -vf "drawtext=fontsize=60: box=1: x=(w-text_w)/2:y=(h-text_h)/2: text='$8'" -t "$duration" -r "$fps" -video_track_timescale "$timescale" -shortest "$output"

output_remuxed="${output%.*}_remuxed.mp4"
ffmpeg -y -i "$output" -c copy -output_ts_offset $start_pts "$output_remuxed"

mv $output_remuxed $output
echo "Conversion complete. Output video: $output"
