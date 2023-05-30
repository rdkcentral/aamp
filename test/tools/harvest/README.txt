This is a tool for harvesting stream content

harvest.py --help 


General method to harvest some content and playback:

1) Make a directory to store harvested content:
$ mkdir harvested_content
$ cd harvested_content

2) Invoke harvest.py to write content into that directory and wait until it finishes.
:.../harvested_content$ ~/aamp/test/tools/harvest/harvest.py https://some_server/some_path/some_manifest_file.m3u8

3) If planning to transcode then fetch a clear 'donor' video. In this case big_buck_bunny.mp4

4) Transcode the content
:.../harvested_content$ ~/aamp/test/tools/replace_segments/transcode.py -a --transcode big_buck_bunny.mp4 some_path/some_manifest_file.m3u8

5) Start simlinear.py to serve the content. In this case we are serving HLS content (.m3u8)
:.../harvested_content$ ~/aamp/test/simlinear/tools/simlinear.py --hls 8085

6) Determine the URL to the content that simlinear will serve. In this example we know simlinear is running on the local 
host and was assigned port 8085
http://127.0.0.1:8085/some_path/some_manifest_file.m3u8

7) start aamp-cli and present it with that URL
...
[AAMPCLI] Enter cmd: http://127.0.0.1:8085/some_path/some_manifest_file.m3u8


See also .../library/test_toolchain.py for regression test of harvest,transcode,playback
after modifications. 



