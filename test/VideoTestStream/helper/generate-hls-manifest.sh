generate_hls_manifest() {
	oldCodec="avc1.4d400d,mp4a.40.2"
	
	if [ "$AUDIO_CODEC" == "aac" ]; then
		newAudioCodec="mp4a.40.2"
	elif [ "$AUDIO_CODEC" == "eac3" ]; then
		newAudioCodec="ec-3"
	elif [ "$AUDIO_CODEC" == "ac3" ]; then
		newAudioCodec="ac-3"
	fi

	if [ "$VIDEO_CODEC" == "h264" ]; then
		newVideoCodec="avc1.4d400d"
	elif [[ "$VIDEO_CODEC" = *"hev1"* ]]; then
		echo "hev1"
		newVideoCodec="hev1.1.2.L93.B0"
	elif [ "$VIDEO_CODEC" == "hevc" ]; then
		newVideoCodec="hvc1.1.6.L93.90"
	fi

	newCodec="$newVideoCodec,$newAudioCodec"
	while IFS= read -r line
	do

		if [[ $line = *"$oldCodec"* ]]; then
                        echo "${line//$oldCodec/$newCodec}"
		else
			echo "$line"
		fi
	done < main.m3u8 > mainDynamic.m3u8
	echo "Generated hls main dynamic manifest"
}

generate_hls_manifest
