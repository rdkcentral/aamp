# Gstreamer Test App Overview


## Philosophy:
* Use local output from generate-hls-dash.sh as ingredients by default
* Clean, well documented porting layer with no dependencies on AAMP
* CLI based test harness encapsulating test cases
* Suitable for focused tests, learnings, experiments (across SOC vendors)

## Code
* gst-test.cpp - queuing framework and test code
* gst-port.h - porting layer abstraction
* gst-port.cpp thin C++ wrapper for gstreamer porting layer

## Command Line Interface:

### help
> show available commands
test
### inventory <vodurl>
> load specified (DASH) manifest and generate inventory.sh with sequence of calls to generate-video-segment.sh and genenerate-audio-segment.sh

### load <vodurl>
> load and play content from specified DASH manifest.
> * Good compatibility with minimal, streamlined implementation, able to play most clear vod content
> * Note: Uses pts restamping approach similar to existing dai2 test for fast period transiitions across discontinuities without need for explicit flush
> * Note: Currently has limitations: no ABR, arbitrary AdaptationSet selection, no DRM support, but useful for testing

### format
> enumerate currently configured format and options

### format 0
> inject elementary stream sample data extracted from mp4, processed with mp4demux, a WIP software mp4 demuxer

### format 1
> default; inject whole mp4 segments to be demuxed by gstreamer qtdemux element

### format 2
> inject elementary stream frames extracted from ts, each decorated with pts/dts from pes header. This is intented to parallel AAMP's use of tsprocessor (software demuxer).

### format 3
> inject whole ts segments (demuxed by gstreamer tsdemux element, if available); this works on Ubuntu via gst-plugins-bad/GstTSDemux but not in other builds

### position
> toggle position reporting (default = off)

### rate <newRate>
> apply instantaneous rate change to currently presenting content (does not work on all platforms)

### flush
> reset pipeline

### seek <offset_s>
> specify target media position for load; call before or during playback

### dai1
> multi-period test exercising discontinuity handling with EOS signaling and flush

### dai2
> optimized multi-period test using pts restamping (no EOS signaling, no flush).  Gives smoother period transitions.

### dai3
> multi-period test using GST_SEEK_FLAG_SEGMENT and non-flushing seek


dai1 dai2 and dai3 play back the following sequence, defined in mPeriodInfo[]
	
| from  | duration | resolution | language |
|:-----:|:--------:|:----------:|:--------:|
|  0s   | 4s       | 720P       | English  |
|  0s   | 4s       | 720P       | Spanish  |
|  0s   | 4s       | 720P       | French   |
|  0s   | 4s       | 720P       | English  |
|  0s   | 4s       | 720P       | Spanish  |
|  0s   | 4s       | 720P       | French   |
| 20s   | 10s      | 720P       | French   |
|  0s   |  4s      | 360P       | Spanish  |
| 30s   |  8s      | 1080P      | English  |
|  0s   |  4s      | 480P       | French   |
| 46s   |  8s      | 1080P      | English  |
|  0s   |  4s      | 720P       | Spanish  |

### play
> set pipeline state to playing; can be used to resume playing after pause

### pause
> can be used with manual tests to start playback in paused state, or to change from playing to paused state

### null
> sets pipeline state to null; audio silenced and video goes black

### dump
> generates gst-test.dot in file system

### path <base_path>
> sets the default path for loading test data

> Examples:
```
path ../../test/VideoTestStream
path https://example.com/VideoTestStream
path http://localhost:8080
path file:///home/user/aamp/test/VideoTestStream
```
