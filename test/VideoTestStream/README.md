# Video Test Stream

In this directory the script **generate-hls-dash.sh** can be used to create a video
test stream in HLS and DASH format. The script **startserver.sh** can be used to run
a web server (**server.py**) so that AAMP can play the video test stream.

Multiple video profiles (for ABR) audio and caption languages are supported.

The following streams are generated:
- main.mpd // DASH
- main_mp4.m3u8 // fragmented HLS mp4
- main.m3u8 // HLS ts with demux audio
- main_mux.m3u8 // HLS ts with muxed audio

**generate-hls-dash-4k.sh** is variation on **generate-hls-dash.sh** and is used to generate an animated test pattern with more realistic
segment sizes and includes a 4k resolution profile.

The following streams are generated:
- SegmentTemplate.mpd
- SegmentTimeline.mpd
- SegmentBase.mpd
- SegmentList.mpd
- Muxed.m4u8
- FragmentedMp4.m3u8
- HlsTs.m3u8


## generate-hls-dash.h

To generate the video test stream on Ubuntu for example:

```
$ cd aamp/tests/VideoTestStream
$ ./generate-hls-dash.h
```

### Known Limitations

Separate 'sidecar' caption text files are generated in the text directory and
can be loaded using the 'set textTrack' **aamp-cli** command and displayed using the
subtec-app server. On Apple Macs, some socket buffer size limits need to be
increased, for example:

```
$ sudo sysctl net.local.dgram.maxdgram=102400
$ sudo sysctl net.local.dgram.recvspace=204800
```

The DASH manifest **main.mpd** and HLS master playlists **main.m3u8**, **main_mp4.m3u8** and
**main_mux.m3u8** are not generated. If you change the generated video test stream
parameters used by **generate-hls-dash.sh** you may need to edit these files.


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
