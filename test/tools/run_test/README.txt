

tcp_client.py Detects A/V gaps via gstreamer tcpserversink
run_test.py   Causes aamp-cli to play manifest HLS test sets containing discontinuitys. Checks 
              log messages output from aamp are as expected. For each test gives PASS/FAIL result
              run_test.py --help for options

To run these tests on HW with simlinear running on Ubuntu VM
1)-3) Setup as detailed in "To run these tests on aamp simulator"
4) Start simlinear on a machine that will be serving the manifest
   a) In the case of Oracle VirtualBox Configure VM to have bridged network adaptor so simlinear can be accessed 
   b) Establish IP address of machine that will be running simlinear for this example it was  192.168.1.240 
   c) start simlinear 
pconduit@pconduit-VirtualBox:~$ ./simlinear.py --interface  192.168.1.240
 * Serving Flask app 'simlinear'
 * Debug mode: on
WARNING: This is a development server. Do not use it in a production deployment. Use a production WSGI server instead.
 * Running on http://192.168.1.240:5000
Press CTRL+C to quit

   d) Enable simlinear to serve HLS using curl command in another terminal window.
pconduit@pconduit-VirtualBox:~$ curl -X POST -d '{"port":"8085", "type":"HLS"}' http://192.168.1.240:5000/sim/start   -H 'Content-Type: application/json'
{
  "result": "success"
}

8) To run 'TESTDATA3' with manifest path testdata/m3u8s_video_discontinuity_180s/manifest.1.m3u8. On a shell accessing HW 
A description of each test and path to manifest can be found in run_test.py

root@skyxione:~# curl -d POST http://localhost:9005/as/players/1/action/watchstream?uri=http%3A%2F%2F192.168.1.240%3A8085%2Ftestdata%2Fm3u8s_video_discontinuity_180s%2Fmanifest.1.m3u8


To run these tests on aamp simulator:

1) python3 and required modules installed.
python3 -m pip install pexpect requests flask webargs aiohttp

2) Archive file containing manifest data has been downloaded ( from location where comcast has put it ) and extracted into directory 'testdata' 

3) aamp repository downloaded and aamp-cli built   
This will result in the following directory structure:
pconduit@pconduit-VirtualBox:~$ ls -l

drwxrwxr-x aamp            <-- aamp repository containing a built aamp-cli
drwxrwxr-x testdata        <-- manifest test data obtained from artifactory or other location where it is kept
       
pconduit@pconduit-VirtualBox:~$ ls testdata
audio                           m3u8s_orig                                   m3u8s_paired_discontinuity_audio_late               m3u8s_paired_discontinuity_video_3s_108s  video
m3u8s                           m3u8s_paired_discontinuity_audio_3s_108s     m3u8s_paired_discontinuity_audio_late_108s          m3u8s_video_discontinuity
m3u8s_audio_discontinuity       m3u8s_paired_discontinuity_audio_early       m3u8s_paired_discontinuity_content_transition       m3u8s_video_discontinuity_180s
m3u8s_audio_discontinuity_180s  m3u8s_paired_discontinuity_audio_early_108s  m3u8s_paired_discontinuity_content_transition_108s  m3u8s_vod
      

4) And then run the tests:

pconduit@pconduit-VirtualBox:~$ ./aamp/test/simlinear/run_test/run_test.py
Creating  /home/pconduit/aamp.cfg
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


Details:
Once RDKAAMP-480 is implemented then the default is that aamp-cli runs without
a window to output A/V hence it can be run via jenkins

When running manually from a terminal then A/V output might be useful at the expense of
A/V gap detection. This can be achieved by "run_test.py --aamp_window"

Automated test setup when run_test.py is invoked:

                               testdata
                                  |
                                  V
                            -----------------
                |--launch-> | simlinear.py  |
  ------------  |           -----------------
 |run_test.py|->|                 | http fetch
  ------------  |                 V
        ^       |             -----------               ---------------
        |       |--launch--> | aamp-cli  |----pipe---> | tcp_client.py |--->|
        |                     -----------               ---------------     |
        |                            |                           ^          |
        |                       tcpserversink                    |          |
        |                            | ---------detect A/V gaps- |          |
        |                                                                   |
        |                                                                   |
        |                                                                   |
        | -----------------log messages --<---------------------------------|
