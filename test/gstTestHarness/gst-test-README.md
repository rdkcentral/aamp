# Gstreamer Test App Overview


## Philosophy:
* Use local output from generate-hls-dash.sh as ingredients by default
* No DASH/HLS parsing, no http file fetches by default
* Clean, well documented porting layer with no dependencies on AAMP
* CLI based test harness encapsulating test cases
* Suitable for focused tests, learnings, experiments (across SOC vendors)
* Path to evolve into new foundation for AAMP

## Code
* gst-test.cpp - queuing framework and test code
* gst-port.h - porting layer abstraction
* gst-port.cpp thin C++ wrapper for gstreamer porting layer

## Reference
*  gstreamer test harness; initial development

## Command Line Interface:

### help
> show available commands

### inventory <vodurl>
> load specified DASH manifest, and generate inventory.sh with sequence of calls to generate-video-segment.sh and genenerate-audio-segment.sh

### load <vodurl>
> load and play content from specified DASH manifest.
> * Good compatibility with minimal, streamlined implementation, able to play most clear vod content
> * Note: Uses pts restamping approach similar to existing dai2 test for fast period transiitions across discontinuities without need for explicit flush
> * Note: Currently has limitations: no ABR, arbitrary AdaptationSet selection, no DRM support, seek not yet implemented, but as a milestone, useful for testing
> * Note: playback can be interrupted with existing 'flush' command

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

### abr
> Plays 2s each of 360p, 480p, 720p, 180p, 720p, 480p, 360p video - 14 seconds total.

### stream <from-position-seconds> <to-position-seconds>
> example:
```
    stream 7.8 15.7
```
> plays Spanish audio/video from 7.8 to 15.7, starting and ending mid-fragment. Position reporting should go from 7818 to 15700

### ff
> use 'seek' to play iframe track at 8x @4fps

### ff2
> use 'rate' to play iframe track at 8x @4fps

### rew
> use 'seek' to play iframe track at 8x @4fps

### rew2
> use 'rate' to play iframe track at -8x @4fps

### rew3
> use 'step' to play iframe track at -8x @4fps

### rate <newRate>
> apply instantaneous rate change to currently presenting content (does not work on all platforms)

### dai
> multi-period test exercising discontinuity handling with EOS signaling and flush

### dai2
> optimized multi-period test using pts restamping (no EOS signaling, no flush).  Gives smoother period transitions.

dai and dai2 play back the following sequence, defined in mPeriodInfo[]
	
| from  | duration | resolution | language |
|:-----:|:--------:|:----------:|:--------:|
| 20s   | 10s      | 720P       | French   |
|  0s   |  4s      | 360P       | Spanish  |
| 30s   |  8s      | 1080P      | English  |
|  0s   |  4s      | 480P       | French   |
| 46s   |  8s      | 1080P      | English  |
|  0s   |  4s      | 720P       | Spanish  |

## ffads <step> <delay>
> multi-period trick play test using real ad content
> delta is number of iframes to skip
> delay specifies milliseconds to hold each frame 

| speed | step | delay |
|:-----:|:----:|:-----:|
|     2x|     1|   1000|
|     3x|     1|    333|
|     4x|     1|    500|
|     6x|     1|    333|
|     8x|     1|    250|
|    12x|     2|    333|
|    16x|     2|    250|
|    30x|     4|    266|
|    32x|     4|    250|

### gap <video> <audio>
> play specified 4s gap bookended by 4s audio/video
>  "content" // fill with normal video/audio\n" );
>  "event" // use gstreamer gap event\n" );
>  "skip" // skip injection; let decoder handle gap\n" );

> example: no gap; 12s continuout audio/video
```
    gap content content
```
> example: video missing in middle
```
    gap skip content
```
> example: video missing in middle with explicit gap
```
    gap event content
```
> example: audio missing in middle
```
    gap content skip
```
> example: audio missing in middle with explicit gap 
```
    gap content event
```

## manual tests:
### 360/480/720/1080
> video options
### en/fr/es
> language options

> These commands can be used for video-only playback, audio-only playback, or mixed/matched audio+video.
 
> Example video-only test:

```
flush
360
video
play
```

> Example audio-only test:
```
flush
fr
audio
play
```

Example audio+video:
```
flush
1080
en
video
audio
play
```
 
### sap
> flush and reinject 360p video with french audio, continuing playback from current position seamlessly

### play
> set pipeline state to playing; can be used to resume playing after pause

### pause
> can be used with manual tests to start playback in paused state, or to change from playing to paused state

### null
> sets pipeline state to null; audio silenced and video goes black

### dump
> generates gst-test.dot in file system

### path <base_path>
> sets the default path for loading the test data

### seek <sec>
> flush and reinject 360p video from the new playback position , seek to a new position and continue play.

> Examples:
```
path ../../test/VideoTestStream
path https://example.com/VideoTestStream
path http://localhost:8080
path file:///home/user/aamp/test/VideoTestStream
```
