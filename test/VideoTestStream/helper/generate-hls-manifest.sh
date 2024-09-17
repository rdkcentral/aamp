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

#generate subtitle manifests

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do

if [ "$I" == 0 ]; then
cat <<EOL >> main.m3u8
#EXT-X-MEDIA:TYPE=SUBTITLES,URI="text/${LANG_639_2[$I]}_subs.m3u8",GROUP-ID="subs",LANGUAGE="${LANG_639_3[$I]}",NAME="${LANG_FULL_NAME[$I]}",DEFAULT=YES,AUTOSELECT=YES
EOL
else
cat <<EOL >> main.m3u8
#EXT-X-MEDIA:TYPE=SUBTITLES,URI="text/${LANG_639_2[$I]}_subs.m3u8",GROUP-ID="subs",LANGUAGE="${LANG_639_3[$I]}",NAME="${LANG_FULL_NAME[$I]}",DEFAULT=NO,AUTOSELECT=YES
EOL
fi

if [ "$GEN_TTML" == 1 ]; then

cat <<EOL > text/${LANG_639_2[$I]}_subs.m3u8
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-MEDIA-SEQUENCE:0
#EXT-X-TARGETDURATION:$TEXT_SEGMENT_SEC
#EXT-X-MAP:URI="ttml_${LANG_639_2[$I]}_init.mp4"
EOL

totalTracks=($VIDEO_LENGTH_SEC/$TEXT_SEGMENT_SEC)

for (( T=1; T<=totalTracks; T++ ))
do
if [ "$T" -lt 10 ]; then
	Track="ttml_${LANG_639_2[$I]}_00$T.mp4"
elif [ "$T" -ge 10 ] && [ "$T" -lt 100 ]; then
	Track="ttml_${LANG_639_2[$I]}_0$T.mp4"
else
        Track="ttml_${LANG_639_2[$I]}_$T.mp4"
fi

cat <<EOL >> text/${LANG_639_2[$I]}_subs.m3u8
#EXTINF:$TEXT_SEGMENT_SEC,
$Track
EOL
done

cat <<EOL >> text/${LANG_639_2[$I]}_subs.m3u8
#EXT-X-ENDLIST	
EOL
fi

if [ "$GEN_WEBVTT" == 1 ]; then
cat <<EOL > text/${LANG_639_2[$I]}_subs.m3u8
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-MEDIA-SEQUENCE:0
#EXT-X-TARGETDURATION:$VIDEO_LENGTH_SEC
#EXTINF:$VIDEO_LENGTH_SEC,
${LANG_FULL_NAME[$I]}.vtt
#EXT-X-ENDLIST	
EOL
fi
done

echo "Generated hls main manifest"

