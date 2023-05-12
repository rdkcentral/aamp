#====================================================================================
# Test position increments
#====================================================================================

# Set the tune URL tag to specific URL
SET_VAR_TAGS(TUNE_URL) https://cpetestutility.stb.r53.xcal.tv/multilang/main.mpd

# Fail the script if we ever get an error or blocked state
failon ERROR BLOCKED


##################################
# Tune to test url (default autoplay = 1)
# Let it play and check for increasing position in progress events

tune $TUNE_URL
waitfor 5 TUNED PLAYING

waitfor 2 PROGRESS(,0(500))
waitfor 2 PROGRESS(,1000(500))
waitfor 2 PROGRESS(,2000(500))
waitfor 2 PROGRESS(,3000(500))
waitfor 2 PROGRESS(,4000(500))
waitfor 2 PROGRESS(,5000(500))
waitfor 2 PROGRESS(,6000(500))
waitfor 2 PROGRESS(,7000(500))
waitfor 2 PROGRESS(,8000(500))
waitfor 2 PROGRESS(,9000(500))
waitfor 2 PROGRESS(,10000(500))

stop

