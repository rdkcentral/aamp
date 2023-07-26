### AAMP Smoketest

AAMP Smoketest runs both non-scripted tune tests and scripted test.  

---
Index
---

1. [Running AAMP Smoke Tests](#running-aamp-smoke-tests)
2. [Specify URLs for Smoketest](#specify-urls-for-smoketest)
3. [Using Scripted Smoke Tests](#using-scripted-smoke-tests)
4. [Description of smoketest test cases](#description-of-each-smoketest-test-case)


---

# Running AAMP Smoke Tests

usage:
: ./aamp_smoketest [-v] [-t testname] [-h]
  - -v display video in window [MacOS only]
  - -t specify a testname string that act as a filter to select test cases to run
  - -h this message

If the app is run normally a set of tests will be run for each script along with the usual non-scripted tune tests:
> aamp_smoketest <-v>

The app may be run specifying that just the non-scripted smoketest test be run:
> aamp_smoketest <-v> -t smoketest

The app may be run specifying that just the scripted smoke test cases be run:
> aamp_smoketest <-v> -t scripts

The app may be run specifying that a single smoke test script, for example "audio.smk" be run. This will not as run as a google test and may
be in a different dir to the the main scripts (in which case just specify the full path).
> aamp_smoketest <-v> -t [script filename]

---

# Specify URLs for Smoketest

User can provide specific URLS that are used by the non-scripted smoketest test cases.
The URLs should be saved in a csv file with filename as "smoketest.csv" at either home directory or /opt:
> ~/smoketest.csv
or
> /opt/smoketest.csv

Example content of smoketest.csv:
```
VOD_DASH,<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>
VOD_HLS,<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8>
```

If there is no smoketest.csv, then default URLs will be used:
for DASH: default url = "<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>"
for HLS:  default url = "<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8>"

For scripted smoketest, URLs are provided in a seperate file "scriptTestUrls.txt" to specify a set of URLs with a tag.
Details can be found in the "Using Scripted Smoke Tests" section below.

---

# Using Scripted Smoke Tests

This intention here is to allow creation of a set of scripts that define smoke tests to be run.
The script is a list of instructions and parameters (similar to aamp-cli).

The scripts should be located in a 'smoketest' dir ('~/smoketest' on a PC and '/opt/smoketest' on a box).
This dir should also contain the tagged list of URLs.

The scripts will be run in alphabetical order (This should allow a set of scripts to be created to test that basic
functionality works before running more complicated tests. The following tests can then be aborted if these fail.)

The actual testing is performed by checking the events sent from aamp using a 'waitfor <EVENT>' command.
Instructions and parameters on a line must be separated by a single space.
Comments may be added by starting the line with '#'

General syntax:

- each command is followed by a space separated list of arguments:
   COMMAND ARG1 ARG2 ...
- an argument may have associated parameters, these should be within brackets:
   COMMAND ARG1(param) ARG2 ...
- if an argument has multiple parmeters, these should be entered as a comma separated list:
   COMMAND ARG1(param1, param2, param3 ...) ARG2 ...
- an parameter may have its own parmeters:
   COMMAND ARG1(param1(p1, p2...), param2, param3 ...) ARG2 ...

- Spaces are not supported other than to separate arguments (within a parameter or parameter list they will be stripped)
- Optionally, a parameter may be represented as ARG:PARAM. This can make some things more readable but a list of
   parameters must be within brackets.
   E.g. to set the language configuration we use the 'config' command with a list of arguments for each item we want to set:
     config preferredAudioLanguage(spa) preferredAudioRendition(french)
   coud also be:
     config preferredAudioLanguage:spa preferredAudioRendition:french
   but for a list we need brackets:
     config preferredAudioLanguage(eng, fra, spa) preferredAudioRendition:french

## Instruction set

- select <player (integer index of player, 0 = main player)>
- new
- loopstart <loops (integer repeats of section up to 'loopend')>
- loopend
- tune <url> [autoplay, 0 or 1, default 1]   (see Setting Tune URL below)
- stop
- detach
- setrate <rate>
- ff <rate>
- rew <rate>
- pause [position seconds - if set specified call PauseAt(position)]
- play
- seek <position in seconds> [keep paused, 0 or 1, default 0]
      (the position may be specified by $DURATION(offset) to seek to asset duration - optionally adjusted by int offset)
- sleep <seconds>
- setaudiotrack <track>
- settexttrack <track>
- waitfor <time seconds> <list of events>             (see Waiting For Events below)
- waitforseq <time seconds> <list of events in order>  (see Waiting For Events below)
- failon <list of events>     (see Waiting For Events below)
- async <enabled, integer 0 or 1>
- config <list of 'name(value)' config parameters>
      e.g. config suppressDecode(true) supportTLS(2) preferredSubtitleLanguage(fra)
- check <type(valuesettings) where 'type' is the type of check and value is a list of (json) settings 'setting(value)' or 'setting:value'>
supported types:
  * audio
  * text

  e.g. 
  ```check audio(language(eng), codec(mp4a.40.2))
      check audio(language:eng, codec:mp4a.40.2)
  ```
- simlinear - launch the simlinear server
- reset - reset aamp players to initial status (with single player)
- timestart <label> - initialise a time log with name 'label'
- timelog <label> - log the time elapsed since timestart (if already logged to this label, the value will be summed to
                    report an average). Time logs will be reported at the end of the script (and in the gtest report)
- msleep <int ms> - sleep for specified milliseconds.
                    an optional random variation may be specified as 'time(variation)' where the value used will be
                    time +  (random value from -variation to +variation).
    e.g. ```msleep 5000(5000)``` would sleep for 5 +-5 seconds (i.e a random value from 0 - 10s)
  
- SET_TARGET(<list of devices>)
     e.g. SET_TARGET(simulator)
          SET_TARGET(XiOneUK, XiOneDE, XiOneIT)
          SET_TARGET(!XiOneIT)
- SET_VAR_TAGS(<var>) <list of tags>    (see Setting Tune URL below)
- SET_ITERATIONS(<iterations>) [list of names]  (see Setting Tune URL below)

- SET_STOP_ON_ERROR - if this is set then all following scripts will be skipped if this script fails

## Setting The Tune URL


The basic ommand to tune to a URl is 'tune <URL>' e.g.:
```tune <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>```
For flexibility, the URL may also be specified by an environment valiable by using '$', e.g.:
```tune $TUNE_VARIABLE```

However we may want a script to run on multiple URLS as separate tests. This is where it gets a little
complicated and is supported as follows:

- Create a file 'scriptTestUrls.txt' in the smoketest dir.
  This should contain a list of tag names and associated URLs to use, e.g.:
    DASH_URL <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>
    HLS_URL <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8>
    ...
- In the script, the tune URL should be specified as environment variable TUNE_URL:
    tune $TUNE_URL
- At the top of the script (ideally) the variable can be associated with a url tag using SET_VAR_TAGS(<variable>), e.g.:
    SET_VAR_TAGS(TUNE_URL) DASH_URL
  Alternatively (for development) the actual url can be used as a tag e.g.:
    SET_VAR_TAGS(TUNE_URL) <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>
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
> DASH_URL_1 <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream1/main.mpd>
> DASH_URL_2 <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream2/main.mpd>
> HLS_URL_1 <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main1.m3u8>
> HLS_URL_2 <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main2.m3u8>

In the smoke test script we'll tune to a url, check for tuned and playing events, play for 10s,
tune to another url, check for tuned and playing events, play for 10s then stop. We'll run this once
using dash and once using hls streams:

```
# Define two iterations called Dash and HHS

SET_ITERATIONS(2) Dash HLS

# Defined the URLs to use -

#  first test: TUNE_URL1 will be DASH_URL_1 (<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream1/main.mpd>)

#              TUNE_URL2 will be DASH_URL_2 (<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream2/main.mpd>)

#  second test: TUNE_URL1 will be HLS_URL_1 (<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main1.m3u8>)

#               TUNE_URL2 will be HLS_URL_2 (<https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main2.m3u8>)

SET_VAR_TAGS(TUNE_URL1) DASH_URL_1 HLS_URL_1
SET_VAR_TAGS(TUNE_URL2) DASH_URL_2 HLS_URL_2

# Could use this instead of the above during development to just run the script with set URLs

#SET_VAR_TAGS(TUNE_URL1) <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>
#SET_VAR_TAGS(TUNE_URL2) <https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd>

# Tune to first stream

tune $TUNE_URL1
waitfor 5 TUNED PLAYING
sleep 10

# Tune to second stream

tune $TUNE_URL2
waitfor 5 TUNED PLAYING
sleep 10
stop
```

Although this is convoluted, hopefully it will make it easier to create scripts that can be repeated for
multiple stream types using a predefined set of test streams.

## Waiting For Events


The 'waitfor' instruction is the bit that does the tests - it specifies what event(s) we expect to get
from the preceding instructions. This may be a single event or list of events (unordered) and 'waitfor'
can be used as a blocking or non-blocking test. If the check fails, the script will exit as failed.
It can be specified that an event should NOT be received with '!<event>'.
Where an event can check for certain parameters, these can, optionally, be added within brackets, e.g.
If an event can have multiple parameters they can be pravided as a comma separated list e.g.
To wait for a progress event we can do:
    waitfor 2 PROGRESS
To wait for a progress event with particular speed and position we can do:
    waitfor 2 PROGRESS(speed,position)
To ignore a parameter just leave it out. So for a progress event where we ignore the speed but check the position we can do:
    waitfor 2 PROGRESS(,position)

Events supported:
  Most AAMPEventType events (just remove the 'AAMP_EVENT_', e.g. for AAMP_EVENT_SPEED_CHANGED use 'SPEED_CHANGED'):
    waitfor 2 SPEED_CHANGED
  For AAMP_EVENT_STATE_CHANGED events, use the PrivAAMPState enum (drop the 'eSTATE_'). E.g. for a AAMP_EVENT_STATE_CHANGED indicating
  a state change to eSTATE_PAUSED, just use 'PAUSED'.
    waitfor 2 PAUSED

Events which will accept parameters to check for:
  PROGRESS(int speed,int ms position)
  SEEKED(int ms position)
  SPEED_CHANGED(int rate)
  BITRATE_CHANGED(uint bitrate) - 0 indicates the minimum available bitrate, -1 indicates the maximum available bitrate
  
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

---

# Description of Smoketest Test Cases
### non-scripted test cases
---
TEST(dashTuneTest, ManifestDLStartTime)
: To test the manifest download start time is less than 7ms(expected)

TEST(dashTuneTest, ManifestDLEndTime)
: To test the manifest download end time is less than 1s(expected)

TEST(dashTuneTest, ManifestDLFailCount)
: To test the manifest download fail count is zero

TEST(dashTuneTest, VideoPlaylistDLFailCount)
: To test the video playlist download fail count is zero

TEST(dashTuneTest, AudioPlaylistDLFailCount)
: To test the audio playlist download fail count is zero

TEST(dashTuneTest, VideoInitDLFailCount)
: To test the video init download fail count is zero

TEST(dashTuneTest, AudioInitDLFailCount)
: To test the audio init download fail count is zero

TEST(dashTuneTest, VideoFragmentDLFailCount)
: To test the video fragment download fail count is zero

TEST(dashTuneTest, AudioFragmentDLFailCount)
: To test the audio fragment download fail count is zero

TEST(dashTuneTest, DrmFailErrorCode)
: To test the DRM fail error code is zero

TEST(dashTuneTest, PauseState)
: To test the pause state is true

TEST(dashTuneTest, FastForwardState)
: To test the fast forward state is true

TEST(dashTuneTest, PlayState)
: To test the play state is true

TEST(dashTuneTest, RewindState)
: To test the rewind state is true

TEST(hlsTuneTest, ManifestDLStartTime)
: To test the manifest download start time is less than 7ms(expected)

TEST(hlsTuneTest, ManifestDLEndTime)
: To test the manifest download end time is less than 1s(expected)

TEST(hlsTuneTest, ManifestDLFailCount)
: To test the manifest download fail count is zero

TEST(hlsTuneTest, VideoPlaylistDLFailCount)
: To test the video playlist download fail count is zero

TEST(hlsTuneTest, AudioPlaylistDLFailCount)
: To test the audio playlist download fail count is zero

TEST(hlsTuneTest, VideoInitDLFailCount)
: To test the video init download fail count is zero

TEST(hlsTuneTest, AudioInitDLFailCount)
: To test the audio init download fail count is zero

TEST(hlsTuneTest, VideoFragmentDLFailCount)
: To test the video fragment download fail count is zero

TEST(hlsTuneTest, AudioFragmentDLFailCount)
: To test the audio fragment download fail count is zero

TEST(hlsTuneTest, DrmFailErrorCode)
: To test the DRM fail error code  is zero

TEST(hlsTuneTest, PauseState)
: To test the pause state is true

TEST(hlsTuneTest, FastForwardState)
: To test the fast forward state is true

TEST(hlsTuneTest, PlayState)
: To test the play state is true

TEST(hlsTuneTest, RewindState)
: To test the rewind state is true

TEST(liveTuneTest, PauseState)
: To test live pause state is true

TEST(liveTuneTest, PlayState)
: To test live play state is true

TEST(harvesterSmokeTest, test_dash)
: To tune to and harvest the dash URL

TEST(harvesterSmokeTest, test_hls)
: To tune to and harvest the hls URL

### scripted test cases
---
audio.smk
: - First tests check that the config settings work as expected
  - Then switch audio track while playing by specifying audio check the selected audio track is set

seek.smk
: Seek and position test (Create two tests for this script, one with dash content and one with hls)
  - Tune to test url, then jump fwd and back 
  - Same test with seek while paused
  - Seek test with no delay
  - Seek test with no check for events till final seek and play
  - Seek test with no delays, async enabled

frozen_video.smk
: Test position increments
  - Tune to test url (default autoplay = 1)
  - let it play and check for increasing position in progress events

time_to_highest_bitrate.smk
: Test time to reach highest bitrate (Create two tests for this script, one with dash content and one with hls)
  - Run the test 5 times, tune and wait till we hit the highest bitrate
  - tune and wait for up to 30s to reach highest bitrate
  - log (sum) the time since timestart with label 'TimeToHighestBitrate'