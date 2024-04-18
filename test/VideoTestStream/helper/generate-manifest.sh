generate_manifests() {
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

	while IFS= read -r line
	do
		if [[ $line = *"$oldDuration"* ]]; then
			echo "${line//$oldDuration/$newDuration}"
		elif [[ $line = *"$oldVideoCount"* ]]; then
			echo "${line//$oldVideoCount/$newVideoCount}"
		elif [[ $line = *"$oldAudioCount"* ]]; then
			echo "${line//$oldAudioCount/$newAudioCount}"
		else
			echo "${line//$oldTextCount/$newTextCount}"
		fi
	done < main.mpd > mainduration.mpd
	echo "Generated mainduration manifest"
}

generate_manifests
