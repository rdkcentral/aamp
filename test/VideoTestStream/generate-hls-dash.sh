#!/bin/bash
source helper/install-dependencies.sh
source helper/config.sh

mkdir $HLS_OUT
mkdir $DASH_OUT
mkdir temp

source helper/generate-video.sh
source helper/generate-iframe-track.sh
source helper/generate-audio-data.sh
source helper/generate-audio-manifests.sh
source helper/generate-muxed-video.sh
