#!/bin/bash

if [[ $((VIDEO_LENGTH_SEC%60)) != 0 ]]; then
	duration="PT$((VIDEO_LENGTH_SEC/60))M0.$((VIDEO_LENGTH_SEC%60))S"
else
	duration="PT$((VIDEO_LENGTH_SEC/60))M0.0S"
fi

if [ "$AUDIO_CODEC" == "aac" ]; then
	audioCodec="mp4a.40.2"
elif [ "$AUDIO_CODEC" == "eac3" ]; then
	audioCodec="ec-3"
elif [ "$AUDIO_CODEC" == "ac3" ]; then
	audioCodec="ac-3"
fi

if [ "$VIDEO_CODEC" == "h264" ]; then
	videoCodec="avc1.4d4028"
elif [[ "$VIDEO_CODEC" = *"hev1"* ]]; then
	videoCodec="hev1.1.2.L93.B0"
elif [ "$VIDEO_CODEC" == "hevc" ]; then
	videoCodec="hvc1.1.6.L93.90"
fi

AdaptationSetId=0
cat <<EOL > main.mpd
<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xlink="http://www.w3.org/1999/xlink" xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-live:2011" type="static" mediaPresentationDuration="$duration" minBufferTime="PT4.0S">
  <ProgramInformation>
	</ProgramInformation>
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="$AdaptationSetId" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
EOL

for (( I=PROFILE_COUNT-1; I>=0; I-- ))
do

  cat <<EOL >> main.mpd
	  <Representation id="${HEIGHT[$I]}p" mimeType="video/mp4" codecs="$videoCodec" bandwidth="${KBPS[$I]}000" width="${WIDTH[$I]}" height="${HEIGHT[$I]}" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/${HEIGHT[$I]}p_init.m4s" media="dash/${HEIGHT[$I]}p_\$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="$((12800*$VIDEO_SEGMENT_SEC))" r="$((VIDEO_LENGTH_SEC/$VIDEO_SEGMENT_SEC - 1))" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
EOL
done

cat <<EOL >> main.mpd
	</AdaptationSet>
EOL

((AdaptationSetId++))
cat <<EOL >> main.mpd
	<AdaptationSet id="$AdaptationSetId" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und"><EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="1"/>
		<Representation id="4" mimeType="video/mp4" codecs="$videoCodec" bandwidth="800000" width="640" height="360" frameRate="1/1">
			<SegmentTemplate timescale="12800" initialization="dash/iframe_init.m4s" media="dash/iframe_\$Number%03d$.m4s" startNumber="1">
				<SegmentTimeline>
			  		<S t="0" d="$((12800*$VIDEO_SEGMENT_SEC))" r="$((VIDEO_LENGTH_SEC/$IFRAME_CADENCE_SEC - 1))" />
				</SegmentTimeline>
			</SegmentTemplate>
		</Representation>
	</AdaptationSet>
EOL

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do

  ((AdaptationSetId++))
  cat <<EOL >> main.mpd
	<AdaptationSet id="$AdaptationSetId" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="${LANG_639_3[$I]}">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="${LANG_FULL_NAME[$I]}" mimeType="audio/mp4" codecs="$audioCodec" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/${LANG_639_2[$I]}_init.m4s" media="dash/${LANG_639_2[$I]}_\$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
				<S t="0" d="95232" />
			  	<S d="96256" r="$((VIDEO_LENGTH_SEC/$AUDIO_SEGMENT_SEC - 2))" />
			  	<S d="78336" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
EOL

  ((AdaptationSetId++))
  cat <<EOL >> main.mpd
	<AdaptationSet id="$AdaptationSetId" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="${LANG_639_3[$I]}">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="commentary"/>
	  <Representation id="${LANG_FULL_NAME[$I]} commentary" mimeType="audio/mp4" codecs="$audioCodec" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/${LANG_639_2[$I]}_init.m4s" media="dash/${LANG_639_2[$I]}_\$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			 	<S t="0" d="95232" />
			  	<S d="96256" r="$((VIDEO_LENGTH_SEC/$AUDIO_SEGMENT_SEC - 2))" />
			  	<S d="78336" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
EOL
done

if [ $GEN_WEBVTT = 1 ] ; then
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do

  ((AdaptationSetId++))
  cat <<EOL >> main.mpd
	<AdaptationSet id="$AdaptationSetId" contentType="text" segmentAlignment="true" lang="${LANG_639_3[$I]}">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
	  <Representation id="${LANG_FULL_NAME[$I]} WebVTT captions" mimeType="text/vtt" codecs="wvtt" bandwidth="400">
		<SegmentTemplate timescale="48000" media="dash/${LANG_639_2[$I]}_\$Number%03d$.vtt" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="$((48000*$TEXT_SEGMENT_SEC))" r="$((VIDEO_LENGTH_SEC/$TEXT_SEGMENT_SEC - 1))" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
EOL
done
fi

if [ $GEN_TTML = 1 ] ; then

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
  ((AdaptationSetId++))
  cat <<EOL >> main.mpd
	<AdaptationSet id="$AdaptationSetId" contentType="text" segmentAlignment="true" lang="${LANG_639_3[$I]}">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="caption"/>
	  <Representation id="${LANG_FULL_NAME[$I]} TTML captions" mimeType="application/mp4" codecs="stpp" bandwidth="400">
		<SegmentTemplate timescale="48000" initialization="text/ttml_${LANG_639_2[$I]}_init.mp4" media="text/ttml_${LANG_639_2[$I]}_\$Number%03d$.mp4" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="$((48000*$TEXT_SEGMENT_SEC))" r="$((VIDEO_LENGTH_SEC/$TEXT_SEGMENT_SEC - 1))" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
EOL
done
fi

cat <<EOL >> main.mpd
  </Period>
</MPD>
EOL

echo "Generated dash main manifest"
