Various tools for processing manifests. More detail in each sub directory


simlinear/ A webserver for serving HLS/DASH streams.
harvest/   A tool for capturing HLS/DASH streams and saving the contents locally
library/   Common modules used by other tools

run_test/  Causes aamp-cli to play manifest HLS test sets containing discontinuitys. Checks 
           log messages output from aamp are as expected. For each test gives PASS/FAIL result
           Probably needs moving to aamp/test/l2test/
pts_restamp_check/	Python script to check buffer restamping
