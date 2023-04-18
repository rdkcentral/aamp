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



