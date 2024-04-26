generate_dash_manifest() {
	oldDuration=PT15M0.0S

	if [[ $((VIDEO_LENGTH_SEC%60)) != 0 ]]; then
		newDuration="PT$((VIDEO_LENGTH_SEC/60))M0.$((VIDEO_LENGTH_SEC%60))S"
	else
		newDuration="PT$((VIDEO_LENGTH_SEC/60))M0.0S"
	fi

	oldVideoCount=448
	newVideoCount=$((VIDEO_LENGTH_SEC/2))
	oldAudioCount=446
	newAudioCount=$((VIDEO_LENGTH_SEC/2))
	oldTextCount=449
	newTextCount=$((VIDEO_LENGTH_SEC/2))

	oldAudioCodec="mp4a.40.2"
	
	if [ "$AUDIO_CODEC" == "aac" ]; then
		newAudioCodec="mp4a.40.2"
	elif [ "$AUDIO_CODEC" == "eac3" ]; then
		newAudioCodec="ec-3"
	elif [ "$AUDIO_CODEC" == "ac3" ]; then
		newAudioCodec="ac-3"
	fi
	
	if [ "$VIDEO_CODEC" == "h264" ]; then
                newVideoCodec="avc1.4d4028"
        elif [[ "$VIDEO_CODEC" = *"hev1"* ]]; then
                newVideoCodec="hev1.1.2.L93.B0"
        elif [ "$VIDEO_CODEC" == "hevc" ]; then
                newVideoCodec="hvc1.1.6.L93.90"
        fi

	while IFS= read -r line
	do
		if [[ $line = *"$oldDuration"* ]]; then
			echo "${line//$oldDuration/$newDuration}"
		elif [[ $line = *"$oldVideoCount"* ]]; then
			echo "${line//$oldVideoCount/$newVideoCount}"
		elif [[ $line = *"$oldAudioCount"* ]]; then
			echo "${line//$oldAudioCount/$newAudioCount}"
		elif [[ $line = *"$oldTextCount"* ]]; then
                        echo "${line//$oldTextCount/$newTextCount}"
		elif [[ $line = *"$oldAudioCodec"* ]]; then
                        echo "${line//$oldAudioCodec/$newAudioCodec}"
		elif [[ $line = *"avc1.4d4028"* ]]; then
                        echo "${line//"avc1.4d4028"/$newVideoCodec}"
		elif [[ $line = *"avc1.4d401f"* ]]; then
                        echo "${line//"avc1.4d401f"/$newVideoCodec}"
		elif [[ $line = *"avc1.4d401e"* ]]; then
                        echo "${line//"avc1.4d401e"/$newVideoCodec}"
		elif [[ $line = *"avc1.4d4016"* ]]; then
                        echo "${line//"avc1.4d4016"/$newVideoCodec}"
		else
			echo "$line"
		fi
	done < main.mpd > mainDynamic.mpd
	echo "Generated dash main Dynamic manifest"
}

generate_dash_manifest
