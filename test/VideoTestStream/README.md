# Video Test Stream

In this directory the script **generate-hls-dash.sh** can be used to create a video
test stream in HLS and DASH format. 
The script **startserver.sh** can be used to run a web server (**server.py**) 
In AAMP give the url to play the generated video test stream.

Multiple video profiles (for ABR) audio and caption languages are supported.

The following streams are generated:
- main.mpd // DASH SegmentTemplate 
- main_mp4.m3u8 // fragmented HLS mp4
- main.m3u8 // HLS ts with demux audio
- main_mux.m3u8 // HLS ts with muxed audio

DASH manifests including SCTE-35 signals for dynamic ad insertion can also be generated.

## generate hls and dash stream

To generate the video test stream on Ubuntu:

```
$ cd aamp/tests/VideoTestStream
$ ./generate-hls-dash.sh
```

##Options:

./generate-hls-dash.sh -h		//show help message
./generate-hls-dash.sh -d 120		//generate video for given duration (default is 60 secs)
./generate-hls-dash.sh -f imagename	//generate video with given image, Image format can be ".jpg" ".jpeg" and ".png". Image resolution to be matched with video resolution(1920x1080)
./generate-hls-dash.sh -l 0		//to disable hls stream generation
./generate-hls-dash.sh -a 0		//to disable dash stream generation
./generate-hls-dash.sh -w  		//to generate webvtt text track (default: 0 disabled)"
./generate-hls-dash.sh -k 		//to generate 4k content (default 1) (should be deprecated)

## Audio and Video codecs supported
Can change audio video codec in helper/config.sh

Audio codecs supported: 
AAC 	 - "aac" 
DOLBYAC3 - "ac3" 
DDPLUS 	 - "eac3"

Video codecs supported: 
H264	    - "h264" 
HEVC(hvc1.) - "hevc" 
HEVC(hev1.) - "hevc -tag:v hev1" 

## Text Track support
Default text track is TTML, generated for all languages given in helper/config.sh for HLS and Dash stream
To disable TTML in helper/config.sh set GEN_TTML=0.

To enable webvtt in helper/config.sh set GEN_WEBVTT=1
To enable webvtt when running generate-hls-dash.sh give option -w 
./generate-hls-dash.sh -w 

## Known Limitations

Separate 'sidecar' caption text files are generated in the text directory and
can be loaded using the 'set textTrack' **aamp-cli** command and displayed using the
subtec-app server. On Apple Macs, some socket buffer size limits need to be
increased, for example:

```
$ sudo sysctl net.local.dgram.maxdgram=102400
$ sudo sysctl net.local.dgram.recvspace=204800
```

## startserver.sh

The script **startserver.sh** can be used to run a web server (**server.py**) so that
AAMP can play the the generated video test stream. Options can be passed to the
server to enable the emulation of an ongoing event (i.e. content growing over
time) or, for HLS, a live event (i.e. a content window).

After generating the video test stream you can play the HLS video test stream on
Ubuntu for example:

```
$ cd aamp/tests/VideoTestStream
$ ./startserver.sh
```

In another terminal window:
```
$ cd aamp
$ LD_LIBRARY_PATH=./Linux/lib ./Linux/bin/aamp-cli http://127.0.0.1:8080/main.m3u8
```

The following URIs are supported:

- http://127.0.0.1:8080/main.mpd
-- DASH stream.
- http://127.0.0.1:8080/main.m3u8
-- HLS TS format stream.
- http://127.0.0.1:8080/main_mux.m3u8
-- HLS TS format muxed audio and video stream.
- http://127.0.0.1:8080/main_mp4.m3u8
-- HLS MP4 format stream.

Press Ctrl-C to terminate the server.

### Server Options

A number of server options are available including some which modify files on
the fly as they are downloaded by AAMP. These include:

--event

Emulate an ongoing event where the available content grows over time.

--live

Emulate a live event where a moving window of content is available. HLS only.

The --help option lists all of the server options. For example:

```
$ ./startserver.sh --help
usage: server.py [-h] [--vod | --event | --live] [--time] [--discontinuity]
                 [--hostname HOSTNAME] [--port PORT] [--mintime MINTIME]
                 [--livewindow LIVEWINDOW] [--minupdate MINUPDATE]
                 [--maxduration MAXDURATION] [--all]

AAMP video test stream HTTP server

options:
  -h, --help            show this help message and exit
  --vod                 VOD test stream (default)
  --event               emulate an event test stream
  --live                emulate a live test stream
  --time                add EXT-X-PROGRAM-DATE-TIME tags to HLS (or live)
                        event playlists (enabled for live)
  --discontinuity       add EXT-X-DISCONTINUITY tags to HLS event playlists
  --hostname HOSTNAME   server socket host name (default localhost)
  --port PORT           HTTP server port number
  --mintime MINTIME     starting event (or live) duration in seconds (default
                        10)
  --livewindow LIVEWINDOW
                        live window in seconds (default 30)
  --minupdate MINUPDATE
                        minimum update period in seconds for DASH live or
                        event (default 6)
  --maxduration MAXDURATION
                        maximum duration of the stream in seconds (DASH only,
                        default 0 for no maximum)
  --all                 enable GET of all files. By default, only files with
                        expected extensions will be served
```

Parameters can be set using the requested URL. For example:

```
http://127.0.0.1:8080/main.mpd?live=true
```

### Testing

A number of server unit tests can be run as follows:

```
$ tests/test_server.py
```

### Known Limitations

Only run the server in a trusted development environment.

HTTPS is not supported.

The web server **server.py** makes certain assumptions about the format of video
test stream files. If these files are changed then the results may be
unexpected when using event, live emulation or other options which modify on the
fly the files downloaded by AAMP.

By default, only files with certain filename extensions will be served by the
web server. Use the --all option to support all filename extensions.

By default, only locally run applications can use the web server **server.py**.
Use the --hostname 0.0.0.0 option to allow access from applications running on
other machines.

# SCTE-35 Ad Insertion

DASH manifests including SCTE-35 signals for dynamic ad insertion can be
generated and played by AAMP using the server.

## Generating the DASH manifest

Create a JSON file with a description of the ad break. For example,
**tests/adbreak.json**:

```
[
 {"type":"Break Start", "time":30.0, "duration":34.0, "event_id":1},
 {"type":"Provider Advertisement Start", "time":32.0, "duration":30.0, "event_id":2},
 {"type":"Provider Advertisement End", "time":62.0, "event_id":2},
 {"type":"Break End", "time":64.0, "event_id":1}
]
```

This describes a 34 second break containing a 30 second ad with id 2.

On Ubuntu for example, generate a DASH manifest for this using the Python3
script **helper/scte35.py**:

```
$ cd aamp/tests/VideoTestStream
$ helper/scte35.py mpd tests/adbreak.json > adbreak.mpd
```

Merge this with the DASH manifest **main.mpd**:

```
$ helper/manifest.py merge adbreak.mpd main.mpd > main_with_adbreak.mpd
```

## Playing the DASH manifest

To play this stream with ad, you will need to have generated the Video Test
Stream using **generate-hls-dash.sh** as described above. You will also need
the Video Test Stream server which supports DASH live simulation
(see above).

Configure AAMP to enable dynamic ad insertion. For example:

```
$ cat ~/aamp.cfg
client-dai=true
```

For example, on Ubuntu, in one terminal window:

```
$ cd aamp
$ LD_LIBRARY_PATH=./Linux/lib ./Linux/bin/aamp-cli
```

Add an ad to play - actually 30s of the Video Test Stream itself.

```
[AAMPCLI] Enter cmd: advert add http://localhost:8080/main.mpd?maxduration=30 30
```

In a second terminal window, start the server:

```
$ cd aamp/tests/VideoTestStream
$ ./startserver.sh
```

In the first terminal window, quickly start playing the stream with the ad
break simulating a live stream:

```
[AAMPCLI] Enter cmd: http://localhost:8080/main_with_adbreak.mpd?live=true
```

Do this quickly after starting the server, otherwise the playback point will be
too close to the ad break for the ad substitution to succeed.

At 32 seconds, the ad should be played, which is just the test stream starting
from the beginning. After another 30 seconds, playback of the live stream should
resume at position 00:01:02.

Use the exit command to stop AAMP and Ctrl-C to terminate the server.


