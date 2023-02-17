# AAMP Simlinear L2 test
<p> Test used to check /verify the discontinuities during the HLS playback</p>

+ simlinear.py  Webserver for serving manifest data.

+ tcp_client.py Detects A/V gaps via gstreamer tcpserversink

+ run_test.py  Causes aamp-cli to play manifest HLS test sets containing discontinuities.Checks log messages output from aamp are as expected. For each test gives PASS/FAIL result


## Pre-requisites to run this test

1. python3 and required modules installed.

         python3 -m pip install pexpect requests flask webargs aiohttp

2. Before start running test stream artifactory path needs to be specified in TEST_STREAM_PATH

    a. When running inside docker

         echo "TEST_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz" >> .env

    b. When running on ubuntu (outside a docker container)

         export TEST_STREAM_PATH=https://artifactory.host.com/artifactory/stream_data.gz

3. Archive file containing manifest data has been downloaded ( URL: from location where the file is stored ) and extracted into directory 'testdata'

# aamp repository downloaded and aamp-cli built
This will result in the following directory structure:
```
$ ls -l
drwxrwxr-x aamp            <-- aamp repository containing a built aamp-cli
drwxrwxr-x testdata        <-- manifest test data obtained from artifactory or other location where it is kept

$ ls testdata
audio                           m3u8s_orig                                   m3u8s_paired_discontinuity_audio_late               m3u8s_paired_discontinuity_video_3s_108s  video
m3u8s                           m3u8s_paired_discontinuity_audio_3s_108s     m3u8s_paired_discontinuity_audio_late_108s          m3u8s_video_discontinuity
m3u8s_audio_discontinuity       m3u8s_paired_discontinuity_audio_early       m3u8s_paired_discontinuity_content_transition       m3u8s_video_discontinuity_180s
m3u8s_audio_discontinuity_180s  m3u8s_paired_discontinuity_audio_early_108s  m3u8s_paired_discontinuity_content_transition_108s  m3u8s_vod
 ```     
## Run the test
```
$ ./aamp/test/simlinear/run_test/run_test.py
Creating  /home/user/aamp.cfg
Canned live HLS playback. No discontinuity testdata1.txt
http://localhost:5000/sim/start {'port': '8085', 'type': 'HLS'} 200
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.07919120788574219
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.08082246780395508
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.08132719993591309
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.08137369155883789
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.08141303062438965
Event {'expect': 'Video Profile added to ABR', 'min': 0, 'max': 1} occurs at elapsed=0.0814504623413086
Event {'expect': 'AAMP_EVENT_EOS', 'min': 180, 'max': 220, 'end_of_test': True} occurs at elapsed=205.17059183120728
Terminating simlinear
Status rtn=None pid=2643
PASSED Canned live HLS playback. No discontinuity
 ...
```
## Details
* By default aamp-cli runs without a window to output A/V so it can be run via Jenkins

* When running manually from a terminal then A/V output might be useful at the expense of

A/V gap detection. This can be achieved by "run_test.py --aamp_window"

Automated test setup when run_test.py is invoked:

                                   testdata
                                      |
                                      V
                                -----------------
                    |--launch->  | simlinear.py  |
    ------------    |           -----------------
    run_test .py|-> |                | http fetch
    ------------    |                V
        ^           |          -----------           ---------------
        |           |-launch-> | aamp-cli  |--pipe--> | tcp_client.py |---> |
        |                      -----------           ---------------        |
        |                            |                           ^          |
        |                       tcpserversink                    |          |
        |                            | ---------detect A/V gaps- |          |
        |                                                                   |
        |                                                                   |
        |                                                                   |
        | -----------------log messages --<---------------------------------|