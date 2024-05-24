#!/bin/bash

source helper/translations.sh
	    
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
    echo
    FILE=${AUDIO_PATH}/${LANG_639_2[$I]}/full_track.wav
	if [ -f "$FILE" ]; then
    echo "$FILE exists"
    else
        echo ${LANG_639_2[$I]}":"
        mkdir -p $AUDIO_PATH/${LANG_639_2[$I]}
	count=59
	if [ $VIDEO_LENGTH_SEC -lt 60 ]; then
  	   count=$VIDEO_LENGTH_SEC
	fi

	for J in $(seq 0 $count)
        do
            OUT=${AUDIO_PATH}/${LANG_639_2[$I]}/${J}
            TEXT=${TRANSLATIONS[$((101*I+J))]}
            echo $TEXT
            sudo gtts-cli "$TEXT" --lang ${LANG_639_2[$I]} --output ${OUT}.mp3
            # pad with silence
            ffmpeg -hide_banner -y -i  ${OUT}.mp3 -af "apad=pad_dur=1" ${OUT}_temp.wav
            # - cut to 1 second
            ffmpeg -hide_banner -y -i ${OUT}_temp.wav -t 1  ${OUT}.wav
            # cleanup
            sudo rm ${OUT}_temp.wav ${OUT}.mp3
        done

        # populate index.txt
        echo "" > $AUDIO_PATH/${LANG_639_2[$I]}/index.txt

        # Calculate the number of minutes
        minutes=$((VIDEO_LENGTH_SEC/60))

        # Ensure the loop runs at least once
        if [ $minutes -eq 0 ]; then
            minutes=1
        fi

	for J in $(seq 1 $minutes)
        do
	    for K in $(seq 0 $count)
            do
                echo "file ${K}.wav" >> $AUDIO_PATH/${LANG_639_2[$I]}/index.txt
            done
        done
        #generate full track from index.txt
        ffmpeg -hide_banner -y -f concat -i $AUDIO_PATH/${LANG_639_2[$I]}/index.txt -c copy $FILE
    fi
done
