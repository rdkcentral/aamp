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
