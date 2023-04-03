#====================================================================================
# Seek and position test
#====================================================================================

# Create two tests for this script, one with dash content and one with hls
# (with names for each iteration - 'Dash' and 'HLS')
SET_ITERATIONS(2) Dash HLS

# Set the tune URL by tag from the URL file using a dash url for the first pass
# and an hls url for the second
SET_VAR_TAGS(TUNE_URL) DASH_URL HLS_URL

# Fail the script if we ever get an error or blocked state
failon ERROR BLOCKED



##################################
# Tune to test url using an env variable so we can change it for each iteration (default autoplay = 1)
# Wait up to 5s for tuned and playing events
# Play for 5s seek to 20s, play for 5s, seek to 10s, play for 5s

tune $TUNE_URL
waitfor 5 TUNED PLAYING
sleep 5

# jump fwd to 20s
seek 20
# wait for up to 2s for progress event with speed 1 and position 20s (no accuracy specified so use the default +-1s)
waitfor 2 PROGRESS(1,20000)
sleep 5

# jump back to 10s
seek 10
# wait for up to 2s for progress event with speed 1 and position 10s (with accuracy +-0.5s)
waitfor 2 PROGRESS(1,10000(500))
sleep 5

stop


################################## 
# Same test with seek while paused
#
tune $TUNE_URL
waitfor 5 TUNED PLAYING
sleep 5
pause
seek 20 1
waitfor 2 PROGRESS(0,20000(1000))
sleep 5
seek 10 1
waitfor 2 PROGRESS(0,10000(1000))
sleep 5
stop


##################################
# Seek test with no delay
#
tune $TUNE_URL
waitfor 5 TUNED PLAYING
seek 20
waitfor 2 PROGRESS(1,20000(1000))
seek 10
waitfor 2 PROGRESS(1,10000(1000))
stop


##################################
# Seek test with no check for events till final seek and play
#
tune $TUNE_URL
waitfor 5 TUNED PLAYING
seek 20
seek 10
seek 15
waitfor 2 PROGRESS(1,15000(1000))
sleep 5
stop



##################################
# Seek test with no delays, async enabled
#

# enable async
async 1

tune $TUNE_URL
waitfor 5 TUNED PLAYING
seek 20
seek 10
seek 15
waitfor 2 PROGRESS(1,15000(1000))
sleep 5
stop


