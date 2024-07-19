#!/bin/bash

source helper/translations.sh

# Convert seconds to a WebVTT time string
# Parameter is the time in seconds
# Returns a time string of format "hh:mm:ss.000"
function webvtt_time {
    VS=$1
    VM=$((VS/60))
    VS=$((VS - (VM*60)))
    VH=$((VM/60))
    VM=$((VM - (VH*60)))
    printf "%02d:%02d:%02d.000" "$VH" "$VM" "$VS"
}

# Echo a WebVTT cue
# Parameters are the cue start time, end time and text
function webvtt_cue () {
    ES=`webvtt_time $1`
    EE=`webvtt_time $2`
    ET="$3"

    echo "$ES --> $EE"
    echo "$ET"
}

# Echo WebVTT file magic
function webvtt_magic () {
    # Add BOM to the text file
    printf '\xEF\xBB\xBF'
    echo 'WEBVTT'
}

# Generate text sidecar files
for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
    echo "Generating ${LANG_FULL_NAME[$I]} text sidecar data"
    FILE=${TEXT_OUT}/${LANG_FULL_NAME[$I]}.vtt
    if [ -f "$FILE" ]; then
        echo "$FILE exists"
    else
        # Write WebVTT file magic
        webvtt_magic > ${FILE}

        for (( T=0; T<VIDEO_LENGTH_SEC; T++ ))
        do
            echo >> ${FILE}
            TEXT=${TRANSLATIONS[$(((101*I)+(T%60)))]}
            webvtt_cue ${T} $((T + 1)) "${TEXT}" >> ${FILE}
        done
    fi
done

for (( I=0; I<LANGUAGE_COUNT; I++ ))
do
	echo "Generating ${LANG_FULL_NAME[$I]} text segments"
	# Segments are TEXT_SEGMENT_SEC long.
	# Segment 1 is the first segment.
	# The content is VIDEO_LENGTH_SEC long.
	for (( SEG=1; SEG<=(VIDEO_LENGTH_SEC/TEXT_SEGMENT_SEC); SEG++ ))
	do
		FILE=`printf '%s/%s_%03d.vtt' ${DASH_OUT} ${LANG_639_2[$I]} ${SEG}`

			# Write WebVTT file magic
			webvtt_magic > ${FILE}

			# Write WebVTT cues, one per second
			for (( J=0; J<TEXT_SEGMENT_SEC; J++ ))
			do
				# Segments are TEXT_SEGMENT_SEC long.
				# Segment 1 is the first segment.
				T=$((((SEG - 1)*TEXT_SEGMENT_SEC) + J))

				echo >> ${FILE}
				TEXT=${TRANSLATIONS[$(((101*I)+(T%60)))]}
				webvtt_cue ${T} $((T + 1)) "${TEXT}" >> ${FILE}
			done
	done
done

