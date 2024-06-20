#!/bin/bash

cat <<EOL > main_mux.m3u8
#EXTM3U
#EXT-X-VERSION:3

EOL

for (( I=0; I<PROFILE_COUNT; I++ ))
do

cat <<EOL >> main_mux.m3u8
#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=${KBPS[$I]}000,RESOLUTION=${WIDTH[$I]}x${HEIGHT[$I]}"
hls/${HEIGHT[$I]}p_mux.m3u8
EOL
done

cat <<EOL >> main_mux.m3u8

#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360,URI="hls/iframe.m3u8"
EOL

echo "Generated main mux manifest"

