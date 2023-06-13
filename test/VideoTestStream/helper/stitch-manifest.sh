stitch_manifests() {
   # write header
   REP=${HEIGHT[0]}
   ghead -n 16 video/"$REP"/SegmentTemplate.mpd > "$1"
   
   for (( I=0; I<PROFILE_COUNT; I++ ))
   do
      REP=${HEIGHT[$I]}
   # trim first 17 lines
   tail -n +17 video/"$REP"/"$1" > temp/temp.mpd
   # replace representation id
   OLD_ID=id=\"0\"
   NEW_ID=id=\"$REP\"
   OLD_BASE="<BaseURL>"
   NEW_BASE="<BaseURL>video/$REP/"
   COUNT=0
   while read -r line
   do
      if [ $COUNT == 0 ]; then
         echo "${line//$OLD_ID/$NEW_ID}"
      elif [ $COUNT == 1 ]; then
         if [[ "$line" == *"<BaseURL>"* ]]; then
            echo "${line//$OLD_BASE/$NEW_BASE}"
		 else
            echo "<BaseURL>video/$REP/</BaseURL>"
            echo "$line"
		 fi
      else
         echo "$line"
         if [[ "$line" == *"</Representation>"* ]]; then
            break
         fi
      fi
      (( COUNT++ )) || true
   done < temp/temp.mpd >> "$1"
   done
   
   # add adaptation set for iframe track
   # TODO: consolidate impl with above
   echo "		</AdaptationSet>" >> "$1"
   echo "		<AdaptationSet id=\"1\" contentType=\"video\" segmentAlignment=\"true\" bitstreamSwitching=\"true\" lang=\"und\">" >> "$1"
   REP=iframe
   # trim first 17 lines
   tail -n +17 video/"$REP"/"$1" > temp/temp.mpd
   # replace representation id
   OLD_ID=id=\"0\"
   NEW_ID=id=\"2\"
   OLD_BASE="<BaseURL>"
   NEW_BASE="<BaseURL>video/$REP/"
   COUNT=0
   while read -r line
   do
      if [ $COUNT == 0 ]; then
         echo "<EssentialProperty schemeIdUri=\"http://dashif.org/guidelines/trickmode\" value=\"1\"/>"
         echo "${line//$OLD_ID/$NEW_ID}"
      elif [ $COUNT == 1 ]; then
         if [[ "$line" == *"<BaseURL>"* ]]; then
            echo "${line//$OLD_BASE/$NEW_BASE}"
		 else
            echo "<BaseURL>video/$REP/</BaseURL>"
            echo "$line"
		 fi
      else
         echo "$line"
         if [[ "$line" == *"</Representation>"* ]]; then
            break
         fi
      fi
      (( COUNT++ )) || true
   done < temp/temp.mpd >> "$1"
   # add adaptation set for audio
   REP="en"
   {
      echo "		</AdaptationSet>"
      echo "		<AdaptationSet id=\"2\" contentType=\"audio\" startWithSAP=\"1\" segmentAlignment=\"true\" bitstreamSwitching=\"true\" lang=\""$REP"\">"
      echo "			<BaseURL>video/en/</BaseURL>"
      tail -n +17 video/$REP/"$1"
   } >> "$1"
}
