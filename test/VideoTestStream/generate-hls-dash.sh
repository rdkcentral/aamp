#!/bin/bash

source helper/config.sh

# Function to display help message
display_help() {
  echo "Usage: [-d duration_in_sec] [-f img_name] [-h help]"
  echo "Optional arguments:"
  echo "  -d   duration_in_sec (default: 60)"
  echo "  -f   img_name (default: testpat.jpg)"
  echo "  -a   Enable/disable dash content (default: 1 Enabled)"
  echo "  -l   Enable/disable hls content (default: 1 Enabled)"
  echo "  -h   Display this help message"
  exit 1
}

# Process command-line options
while getopts ":d:f:h:a:l" opt; do
  case $opt in
    d)
      VIDEO_LENGTH_SEC="$OPTARG"
      ;;
    f)
      IMG_NAME="$OPTARG"      
      ;;
    a)
      RUN_DASH="$OPTARG"
      ;;
    l)
      RUN_HLS="$OPTARG"
      ;;
    h)
      display_help
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      display_help
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      display_help
      ;;
  esac
done

# Shift the processed options, so $1, $2, etc., point to the remaining arguments
shift $((OPTIND-1))

source helper/install-dependencies.sh

mkdir $HLS_OUT
mkdir $DASH_OUT
mkdir temp
mkdir $TEXT_OUT

source helper/generate-video.sh
source helper/generate-iframe-track.sh
source helper/generate-audio-data.sh
source helper/generate-audio-manifests.sh
source helper/generate-text-data.sh
if [ "$RUN_HLS" == 1 ]; then
	source helper/generate-muxed-video.sh
	source helper/generate-hls-manifest.sh
fi

if [ "$RUN_DASH" == 1 ]; then
	source helper/generate-dash-manifest.sh
fi
