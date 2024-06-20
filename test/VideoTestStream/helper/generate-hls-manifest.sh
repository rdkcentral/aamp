#!/bin/bash

if [ "$AUDIO_CODEC" == "aac" ]; then
	audioCodec="mp4a.40.2"
elif [ "$AUDIO_CODEC" == "eac3" ]; then
	audioCodec="ec-3"
elif [ "$AUDIO_CODEC" == "ac3" ]; then
	audioCodec="ac-3"
fi

if [ "$VIDEO_CODEC" == "h264" ]; then
	videoCodec="avc1.4d400d"
elif [[ "$VIDEO_CODEC" = *"hev1"* ]]; then
	echo "hev1"
	videoCodec="hev1.1.2.L93.B0"
elif [ "$VIDEO_CODEC" == "hevc" ]; then
	videoCodec="hvc1.1.6.L93.90"
fi


cat <<EOL > main.m3u8
#EXTM3U
#EXT-X-VERSION:3

EOL

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do

if [ $I = 0 ] ; then
cat <<EOL >> main.m3u8
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="main",NAME="${LANG_FULL_NAME[$I]}",LANGUAGE="${LANG_639_3[$I]}",URI="hls/${LANG_639_2[$I]}.m3u8",DEFAULT=YES,AUTOSELECT=YES
EOL
else
cat <<EOL >> main.m3u8
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="main",NAME="${LANG_FULL_NAME[$I]}",LANGUAGE="${LANG_639_3[$I]}",URI="hls/${LANG_639_2[$I]}.m3u8",DEFAULT=NO,AUTOSELECT=NO
EOL
fi
cat <<EOL >> main.m3u8
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="commentary",NAME="${LANG_FULL_NAME[$I]} commentary",LANGUAGE="${LANG_639_3[$I]}",URI="hls/${LANG_639_2[$I]}.m3u8",DEFAULT=NO,AUTOSELECT=NO
EOL
done

cat <<EOL >> main.m3u8

EOL

for (( I=0; I<PROFILE_COUNT; I++ ))
do

cat <<EOL >> main.m3u8
#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="main",BANDWIDTH=${KBPS[$I]}000,RESOLUTION=${WIDTH[$I]}x${HEIGHT[$I]},CODECS="$videoCodec,$audioCodec"
hls/${HEIGHT[$I]}p.m3u8
EOL
done

cat <<EOL >> main.m3u8

EOL

for (( I=0; I<PROFILE_COUNT; I++ ))
do

cat <<EOL >> main.m3u8
#EXT-X-STREAM-INF:PROGRAM-ID=1,AUDIO="commentary",BANDWIDTH=${KBPS[$I]}000,RESOLUTION=${WIDTH[$I]}x${HEIGHT[$I]},CODECS="$videoCodec,$audioCodec"
hls/${HEIGHT[$I]}p.m3u8
EOL
done

cat <<EOL >> main.m3u8

#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=800000,RESOLUTION=640x360,CODECS="$videoCodec",URI="hls/iframe.m3u8"
EOL

echo "Generated hls main manifest"

