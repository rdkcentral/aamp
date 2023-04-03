Using Scripted Smoke Tests
==========================

This intention here is to allow creation of a set of scripts that define smoke tests to be run.
The script is a list of instructions and parameters (similar to aamp-cli).
The actual testing is performed by checking the events sent from aamp using a 'waitfor <EVENT>' command.
Instructions and parameters on a line must be separated by a single space.
Comments may be added by starting the line with '#'


Instruction set:
-----------------

  select <player (integer index of player, 0 = main player)>
  new
  loopstart <loops (integer repeats of section up to 'loopend')>
  loopend
-  tune <url> [autoplay, 0 or 1, default 1] 		(see Setting Tune URL below)
  stop
  detach
  setrate <rate>
  ff <rate>
  rew <rate>
  pause
  play
  seek <position in seconds> [keep paused, 0 or 1, default 0]
      (the position may be specified by $DURATION(offset) to seek to asset duration - optionally adjusted by int offset)
  sleep <seconds>
  setaudiotrack <track>
  settexttrack <track>
  waitfor <time seconds> <list of events> 		(see Waiting For Events below)
  failon <list of events>		 		(see Waiting For Events below)
  async <enabled, integer 0 or 1>
  config <list of 'name(value)' config parameters>
      e.g. config suppressDecode(true) supportTLS(2) preferredSubtitleLanguage(fra)
    
  SET_VAR_TAGS(<var>) <list of tags> 			(see Setting Tune URL below)
  SET_ITERATIONS(<iterations>) [list of names]		(see Setting Tune URL below)



Setting The Tune URL:
---------------------

The basic ommand to tune to a URl is 'tune <URL>' e.g.:
  tune https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd

For flexibility, the URL may also be specified by an environment valiable by using '$', e.g.:
  tune $TUNE_VARIABLE

However we may want a script to run on multiple URLS as separate tests. This is where it gets a little
complicated and is supported as follows:
- Create a file 'scriptTestUrls.txt' in the smoketest dir.
  This should contain a list of tag names and associated URLs to use, e.g.:
    DASH_URL https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
    HLS_URL https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8
    ...
- In the script, the tune URL should be specified as environment variable TUNE_URL:
    tune $TUNE_URL
- At the top of the script (ideally) the variable can be associated with a url tag using SET_VAR_TAGS(<variable>), e.g.:
    SET_VAR_TAGS(TUNE_URL) DASH_URL
  Alternatively (for development) the actual url can be used as a tag e.g.:
    SET_VAR_TAGS(TUNE_URL) https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
- To create multiple tests with different URLs we can tell the script to run multiple iterations (and optionally 
  provide a name for each iteration for reporting purposes) using SET_ITERATIONS(<iterations>) [name list], e.g.:
    SET_ITERATIONS(2) Dash HLS
  All tag environment variables will also require a tag for each iteration:
    SET_VAR_TAGS(TUNE_URL) DASH_URL HLS_URL
    SET_VAR_TAGS(ADVERT_URL) DASH_AD_URL HLS_AD_URL
    ...
  Each iteration of the script will then set the environment variable to use the next tag along.

As an example, say we want to run a basic tune test, tune to one URL then tune to another, on both Dash 
and HLS streams. The test is the same, but the URLs will differ. To do this, in scriptTestUrls.txt we'll 
need two test urls for each case:    
> DASH_URL_1 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream1/main.mpd
> DASH_URL_2 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream2/main.mpd
> HLS_URL_1 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main1.m3u8
> HLS_URL_2 https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main2.m3u8

In the smoke test script we'll tune to a url, check for tuned and playing events, play for 10s,
tune to another url, check for tuned and playing events, play for 10s then stop. We'll run this once 
using dash and once using hls streams:

> # Define two iterations called Dash and HHS
> SET_ITERATIONS(2) Dash HLS
> # Defined the URLs to use - 
> #  first test: TUNE_URL1 will be DASH_URL_1 (https://cpetestutility.stb.r53.xcal.tv/VideoTestStream1/main.mpd)
> #              TUNE_URL2 will be DASH_URL_2 (https://cpetestutility.stb.r53.xcal.tv/VideoTestStream2/main.mpd)
> #  second test: TUNE_URL1 will be HLS_URL_1 (https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main1.m3u8)
> #               TUNE_URL2 will be HLS_URL_2 (https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main2.m3u8)
> SET_VAR_TAGS(TUNE_URL1) DASH_URL_1 HLS_URL_1
> SET_VAR_TAGS(TUNE_URL2) DASH_URL_2 HLS_URL_2
>
> # Could use this instead of the above during development to just run the script with set URLs:
> #SET_VAR_TAGS(TUNE_URL1) https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
> #SET_VAR_TAGS(TUNE_URL2) https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
>
> # Tune to first stream
> tune $TUNE_URL1
> waitfor 5 TUNED PLAYING
> sleep 10
>
> # Tune to second stream
> tune $TUNE_URL2
> waitfor 5 TUNED PLAYING
> sleep 10
> stop


Although this is convoluted, hopefully it will make it easier to create scripts that can be repeated for 
multiple stream types using a predefined set of test streams.



Waiting For Events:
-------------------

The 'waitfor' instruction is the bit that does the tests - it specifies what event(s) we expect to get
from the preceding instructions. This may be a single event or list of events (unordered) and 'waitfor' 
can be used as a blocking or non-blocking test. If the check fails, the script will exit as failed.
It can be specified that an event should NOT be received with '!<event>'.
Where an event can check for certain parameters, these can, optionally, be added within brackets, e.g.
To wait for any AUDIO_TRACKS_CHANGED event we can do:
    waitfor 2 AUDIO_TRACKS_CHANGED
To wait for an AUDIO_TRACKS_CHANGED and check the track number we can do
    waitfor 2 AUDIO_TRACKS_CHANGED(track number)
If an event an have multiple parameters they can be pravided as a comma separated list e.g.
To wait for a progress event we can do:
    waitfor 2 PROGRESS
To wait for a progress event with particular speed and position we can do:
    waitfor 2 PROGRESS(speed,position)
To ignor a parameter just leave it out so for a progress event wher we ignore the speed but check the position we can do:
    waitfor 2 PROGRESS(,position)

Events currently supported:
  TUNED (includes checking for TUNE_FAILED)
  TUNE_FAILED
  PLAYING
  PAUSED
  STOPPED
  ERROR
  BLOCKED
  COMPLETE
  EOS
  AUDIO_TRACKS_CHANGED(int track index)
  TEXT_TRACKS_CHANGED(int track index)
  PROGRESS (int speed,int ms position)

Note:
  - the accuracy of the postion (specified in ms) is by default 1s but may be specifed by appending '(accuracy)' e.g.:
      waitfor 1 PROGRESS(,20000(500))
    will check for a progress event within the next 1s, ignore the speed and check for a position of 20s (+/-500ms)

Blocking:
 If 'waitfor' is specified with a non-zero time then the script will block for up that number of seconds waiting
 for the specified events to arrive, e.g.:
   waitfor 5 TUNED PLAYING
 will wait up to 5s for the TUNED and PLAYING events (in any order).

Non-blocking:
 If 'waitfor' is specified with a zero time then the events will be monitored as the script continues. This must
 be followed by a further 'waitfor' to check to see it the events arrived (with or without a blocking wait time) e.g:
   waitfor 0 PAUSED EOS
   .... do something ....
   waitfor 2
 will look out for PAUSED and EOS while it does something and then if they haven't arrived already, wait up to 2 seconds
 for them (0 may be used to check without waiting)

The 'failon' comand can be used to set a list of events that, if any occur, will cause the script to fail, e.g.:
   failon ERROR BLOCKED

The '!' operator can be used to check an event does not occur. E.g. to check an EOS does not occur in the next 5s:
   waitfor 5 !EOS

   
   
Running scripted smoke tests
============================

If the app is run normally a set of tests will be run for each script along with the usual tune tests:
    aamp_smoketest <-v>
    
The app may be run specifying that just the smoke test scripts be run:
    aamp_smoketest <-v> -t scripts

The app may be run specifying that a single smoke test scripts be run (not as a google test):
    aamp_smoketest <-v> -t <script filename>

