# AAMP L2 test suite
<p>Test suite used to check/verify the PTS Re-Stamping</p>

## Test Scenarios

### Test 0  (Playback scenario that validates Video/Audio is restamped correctly)
### Test 1  (Trickplay scenario that validates that the I-frame track is restamped correctly)

## Run l2test using script:

From the *test/l2test/ folder run:

source l2venv/bin/activate
./run_l2_aamp.py -v -t 8003

To run a test individually, use the -k parameter, which is forwarded to pytest:
./run_l2_aamp.py -v -t 8003 -k 8003_0
