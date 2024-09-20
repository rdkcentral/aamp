##AAMP DASH Multiview Playback Tests

This set of L2 tests aims to verify various functionalities related to dash multiview, including seamless audio switching, codec selection, and subtitle handling.

##Test Files

test_5000.py -> Iterates through test cases to verify various functionalities related to seamless audio track switching, codec switching, and subtitle handling during playback.

##Pre-requisites to Run This Test

1. Set up the L2 test environment
Follow the detailed instructions in /test/l2test/README.md (this is a one-time setup).

2. Activate the Python virtual environment
Each time the L2 tests need to be run, activate the virtual environment:

    source l2venv/bin/activate

##Running the L2 Tests Using the Script

This test will take around 15-20 seconds to complete. Use the following command to run the tests from the test/l2test/ folder:

./run_l2_aamp.py -t 5000

##Test Details

TESTDATA1 -> Tests seamless audio switching functionality using different language tracks.

TESTDATA2 -> Verifies codec switching during playback, ensuring that the audio properly switches between alternate codecs (e.g., mp4a.40.5 and ec-3).

TESTDATA3 -> Tests subtitle track selection and handling, ensuring that subtitles are correctly selected.