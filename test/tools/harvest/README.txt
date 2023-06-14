This is a tool for harvesting stream content

harvest.py --help 


General method to harvest some content and playback:

1) Make a directory to store harvested content:
$ mkdir harvested_content
$ cd harvested_content

2) Invoke harvest.py to write content into that directory and wait until it finishes.
:.../harvested_content$ ~/aamp/test/simlinear/harvest/harvest.py https://some_server/some_path/some_manifest_file.m3u8

3) Start simlinear.py to serve the content. In this case we are serving HLS content (.m3u8)
:.../harvested_content$ ~/aamp/test/simlinear/tools/simlinear.py --hls 8085

4) Determine the URL to the content that simlinear will serve. In this example we know simlinear is running on the local 
host and was assigned port 8085
http://127.0.0.1:8085/some_path/some_manifest_file.m3u8

5) start aamp-cli and present it with that URL
...
[AAMPCLI] Enter cmd: http://127.0.0.1:8085/some_path/some_manifest_file.m3u8



To test this tool and harvest from some known streams:

pconduit@pconduit-VirtualBox:$ ~/aamp/test/simlinear/harvest/harvest.py --test ccc
['/home/pconduit/aamp/test/simlinear/harvest/harvest.py', '-r', 'harvest_test0', 'https://lin001-gb-s8-tst-ll.cdn01.skycdp.com/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd']
playback URL  http://127.0.0.1:8085/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd

['/home/pconduit/aamp/test/simlinear/harvest/harvest.py', '-r', 'harvest_test1', 'https://752d29ed521d4abe827aa6dd999dd9c5.mediatailor.ap-southeast-2.amazonaws.com/v1/dash/6c977b4f05b6d516365acf5b2c1772257652525a/l2v-pre-mid-post-brian/out/v1/fd1a1f0db1004e3fb6a038d758ad413e/24ecd621af46441296b9379d4904ce23/43240b0b39ec452b8d2a8f1607a0f54a/index.mpd']
playback URL  http://127.0.0.1:8085/v1/dash/6c977b4f05b6d516365acf5b2c1772257652525a/l2v-pre-mid-post-brian/out/v1/fd1a1f0db1004e3fb6a038d758ad413e/24ecd621af46441296b9379d4904ce23/43240b0b39ec452b8d2a8f1607a0f54a/index.mpd
...

To playback the first harvested content which will have be written to harvest_test0 
1) Start simlinear to playback dash content since we can see the URL is for DASH content
cd harvest_test0
~/aamp/test/simlinear/tools/simlinear.py --dash 8085

2) Start aamp-cli and pass it the "playback URL" that was output http://127.0.0.1:8085/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd
...
[AAMPCLI] type 'help' for list of available commands
[AAMPCLI] Enter cmd: http://127.0.0.1:8085/SKYNEHD_HD_SUD_SKYUKD_4050_18_0000000000000018163.mpd




