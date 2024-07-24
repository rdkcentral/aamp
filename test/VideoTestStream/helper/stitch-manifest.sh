stitch_manifests() {
   # write header
   REP=${HEIGHT[4]}
   head -n 13 video/"$REP"/video4k.mpd > "$1"
   
   for (( I=4; I<PROFILE_COUNT; I++ ))
   do
      REP=${HEIGHT[$I]}
   # trim first 17 lines
   tail -n +14 video/"$REP"/video4k.mpd > temp/temp.mpd
   echo $REP
   # replace representation id
   OLD_ID=id=\"0\"
   NEW_ID=id=\"$REP\"
   while IFS= read -r line
   do
	 if [[ "$line" == *"<Representation"* ]]; then
         	echo "${line//$OLD_ID/$NEW_ID}"
		echo "<BaseURL>video/$REP/</BaseURL>"
	 else
            echo "$line"
	 fi

         if [[ "$line" == *"</Representation>"* ]]; then
            break
         fi
   done < temp/temp.mpd >> "$1"
   done
 
 AdaptationSetId=1 
 REP="iframe4k"
  {
    echo "      </AdaptationSet>"
    echo "      <AdaptationSet id=\"$AdaptationSetId\" contentType=\"video\"  segmentAlignment=\"true\" bitstreamSwitching=\"true\"  lang=\"und\">"
    echo "         <EssentialProperty schemeIdUri=\"http://dashif.org/guidelines/trickmode\" value=\"1\"/>"
    echo "        <BaseURL>video/$REP/</BaseURL>"

    # Loop through the lines of iframe.mpd
    print_line=false
    
    # Loop through the lines of iframe.mpd
    while IFS= read -r line; do
        if [[ "$line" == *"<Representation"* ]]; then
            print_line=true  # Start printing when opening tag is found
        fi
        
        if $print_line; then
            echo "$line"  # Output the current line
            
            if [[ "$line" == *"</Representation>"* ]]; then
                break  # Exit the loop if the closing tag is found
            fi
        fi
    done < video/iframe4k/iframe.mpd  # Read from iframe.mpd

   } >> "$1" 

   
# add adaptation set for audio
for lan in "${LANG_639_2[@]}" 
do

((AdaptationSetId++))
 REP="$lan"
   {
    echo "      </AdaptationSet>"
    echo "      <AdaptationSet id=\"$AdaptationSetId\" contentType=\"audio\" startWithSAP=\"1\" segmentAlignment=\"true\" bitstreamSwitching=\"true\" lang=\"$REP\">"
    echo "        <BaseURL>video/$REP/</BaseURL>"

   
    print_line=false
    
   
    while IFS= read -r line; do
        if [[ "$line" == *"<Representation"* ]]; then
            print_line=true  # Start printing when opening tag is found
        fi
        
        if $print_line; then
            echo "$line"  # Output the current line
            
            if [[ "$line" == *"</Representation>"* ]]; then
                break  # Exit the loop if the closing tag is found
            fi
        fi
    done < video/$REP/"$REP".mpd

   } >> "$1" 
done

#  closing tags for the mpd  

   {
      echo "		</AdaptationSet>"
      echo "		</Period>"
      echo "			</MPD>"
   
   } >> "$1"
}
