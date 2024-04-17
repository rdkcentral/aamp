# AAMP Multi profile test

This python3 L2 test verifies offset

<RDKAAMP-2434> L2 Test script generation: Test offset

"offset" value is configured in aamp.cfg as offset=<number>
Started playback of stream
Observed, "offsetFromStart(<number>)" in aamp.log
Assume if you configured 10 as an offset value, "offsetFromStart(10.000000)" is logged in aamp.log 


## Run l2test using script:


## Example:

    cd aamp/test/l2test/
    source l2venv/bin/activate
    python run_l2_aamp.py -t 2011 
