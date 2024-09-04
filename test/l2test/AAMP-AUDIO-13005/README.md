##AAMP DASH Multiview Playback Tests

This set of L2 tests aims to verify functionality related to dash multiview, including audio switching.

##Test Files

test_13005.py -> Iterates through test cases to verify functionality related to audio track switching.

##Pre-requisites to Run This Test

1. Set up the L2 test environment
Follow the detailed instructions in /test/l2test/README.md (this is a one-time setup).

2. Activate the Python virtual environment
Each time the L2 tests need to be run, activate the virtual environment:

    source l2venv/bin/activate

##Running the L2 Tests Using the Script

This test will take around 15-20 seconds to complete. Use the following command to run the tests from the test/l2test/ folder:

./run_l2_aamp.py -t 13005
