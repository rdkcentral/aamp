This is a tool to playback previously harvested manifests

simlinear.py --help

To playback previously harvested content contained in a directory 'content' to the local 
host I.E the aamp simulator will also be running on the same host:

1) Establish the location of the top level manifest E.G content/manifest.mpd

2) Start simlinear to playback dash content since we can see the URL is for DASH content
$ cd content
$ ~/aamp/test/simlinear/tools/simlinear.py --dash 8085

3) Start aamp-cli and pass it the constructed http://127.0.0.1:8085/manifest.mpd
...
[AAMPCLI] type 'help' for list of available commands
[AAMPCLI] Enter cmd: http://127.0.0.1:8085/manifest.mpd

====================================================================================================
simlinear.py (~/aamp/test/tools/simlinear/simlinear.py)

Some manifest contains BaseURL.

Simlinear tries to BaseURL from manifest to serve segments. Since transcoded
segments are available locally, will have to change production BaseURL to simlinear
endpoint.

Some of the some streams has AD-Periods and BaseURL for these periods are
different than the main content BaseURL.

—ad_server http://127.0.0.1:8089 this flag will route the Ad-Periods to the mentioned
domain. If this flag is not passed then AD-Segments will be consumed from
Production endpoint as mentioned in BaseURL.

Added patch to replace BaseURLs with simlinear endpoints.

# Change directory where harvest_details.json is present
cd /home/mutsl02618/Downloads/4-Oct/hevc_tbs_2_dump/dump/

# Run simlinear
python /home/mutsl02618/comcast/aamp/test/tools/simlinear/simlinear.py --dash 8086

# Instance to serve ad segments
# Change directory where harvest_details.json is present
cd /home/mutsl02618/Downloads/4-Oct/hevc_tbs_2_dump/dump/

# Run Simlinear
python /home/mutsl02618/comcast/aamp/test/tools/simlinear/simlinear.py --dash 8089

# Instance to serve main content
# Change directory where harvest_details.json is present
cd /home/mutsl02618/Downloads/4-Oct/hevc_tbs_2_dump/dump/

# Run Simlinear
python /home/mutsl02618/comcast/aamp/test/tools/simlinear/simlinear.py --dash 8086 --ad_server http://127.0.0.1:8089
====================================================================================================
# Modified Response
- Capability to inject delay in responding Segments and/or Manifest files.
- Capability to throw HTTP 404 or HTTP 500 errors for certain request.

# respData Query Parameter
- Create rules to inject delay or to throw HTTP 404, 500 errors.
- Rule Details :
| # | Key | Value | Description |
| 1 | status | 404 | HTTP Error code 404 & 500 is allowed |
| 2 | delay | 5000 | Add delay in response by 5 seconds. Positive Values are expected, max value allowed is 10000. This is time in miliseconds. |
| 3 | pattern | <regular_expression> | This is optional. Matches regex pattern on requested URL. If pattern is provided then delay / status rule will be applicable for matching URLs |
- Rule example : 
    1. Example 1:
    [{"status": 404, "pattern": "720p_00[4-6].m4s"}]
    Here, Simlinear will respond with HTTP 404 error for Segment URLs like 720p_004.m4s 720p_005.m4s 720p_006.m4s.
    2. Example 2 :
    [{"status": 404, "pattern": "480p_01[1-3]"},{"delay": 3000, "pattern": "(1080|720)p_init.m4s"}]
    Here, Simlinear will respond with HTTP 404 error for Segment URLs like 480p_011.m4s 480p_012.m4s 480p_013.m4s and 3 seconds of delay will be added while responding 1080p_init.m4s & 720p_init.m4s
    3. Example 3 :
    [{"status": 404, "start": 0, "end": 2}]
    Here, Simlinear will respond with HTTP 404 error during the first 2 seconds of playback.
- Encode Rule using Base64 encoding technique and pass it in query parameter of respData
    Playback URL for example 1 :
        http://127.0.0.1:8085/manifest.mpd?respData=W3sic3RhdHVzIjogNDA0LCAicGF0dGVybiI6ICI3MjBwXzAwWzQtNl0ubTRzIn1d
    Playback URL for example 2 :
        http://127.0.0.1:8085/output.mpd?respData=W3sic3RhdHVzIjogNDA0LCAicGF0dGVybiI6ICI0ODBwXzAxWzEtM10ifSx7ImRlbGF5IjogMzAwMCwgInBhdHRlcm4iOiAiKDEwODB8NzIwKXBfaW5pdC5tNHMifV0=
    Playback URL for example 3 :
        http://127.0.0.1:8085/manifest.mpd?respData=W3sic3RhdHVzIjogNDA0LCAic3RhcnRfdGltZV9vZmZzZXQiOiAwLCAiZW5kX3RpbWVfb2Zmc2V0IjogMn1d
====================================================================================================
