var audioTracks = [
  {
    "language": "eng",
    "codec": "mp4a.40.2",
    "rendition": "mono",
    "availability": true,
    "index":"0"
  },
  {
    "language": "ger",
    "codec": "mp4a.40.2",
    "rendition": "mono",
    "availability": true,
    "index":"1"
  },
  {
    "language": "spa",
    "codec": "mp4a.40.2",
    "rendition": "mono",
    "availability": true,
    "index":"2"
  },
  {
    "language": "fra",
    "codec": "mp4a.40.2",
    "rendition": "mono",
    "availability": true,
    "index":"3"
  },
  {
    "language": "pol",
    "codec": "mp4a.40.2",
    "rendition": "mono",
    "availability": true,
    "index":"4"
  }
];

const TST_2006_url = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/misc/multilang/main.m3u8";

class AAMPPlayer2 extends AAMPPlayer{
    constructor(player_name) {
        super(player_name);
    }

    async TST_CheckMuteVideo()
    {
        var mute_duration = 10;
        var unmute_duration = 10;
        var capture_duration = 60;

        TST_STEP("Init Vid Capture... ");
        var vid_capture = new VideoCapture();
        if (vid_capture.isActive()) {
            await vid_capture.Init();
            var is_init = await vid_capture.WaitForInit();
            TST_ASSERT ((is_init == true), ("Unexpected Init Status - Got: " + is_init + ", Expected: true"));

            var is_video_present = await vid_capture.WaitForVideoPresent();
            TST_ASSERT ((is_video_present == true), ("Unexpected Video Present Status - Got: " + is_video_present + ", Expected: true"));

            var filename = "VID_000.mp4";
            await vid_capture.Capture(filename, capture_duration);
            var is_capturing = await vid_capture.WaitForCapturing();
            TST_ASSERT ((is_capturing == true), ("Unexpected Capturing Status - Got: " + is_capturing + ", Expected: true"));
        }
        else {
            TST_STEP("Video Capture not in use for this test run");
        }

        // mute the video
        TST_STEP("mute video");
        await this.player.setVideoMute(true);
        await this.VerifyPlayback(mute_duration, 1);

        // unmute the video
        TST_STEP("unmute video");
        await this.player.setVideoMute(false);
        await this.VerifyPlayback(unmute_duration, 1);

        if (vid_capture.isActive()) {
            var assert_message = "";
            var blanking_data = await vid_capture.GetBlankingStatus();
            var blank_periods = blanking_data[0];
            var blank_last_time = blanking_data[1];

            if (blank_periods != 1) {
                assert_message = ("Didn't get expected blank periods, expected: 1 actual: " + blank_periods)
            }
            else if ((blank_last_time < (mute_duration - 1)) || (blank_last_time > (mute_duration + 1))) {
                assert_message = ("Blank duration out of tolerance, expected: " +  mute_duration + " actual: " + blank_last_time)
            }

            await vid_capture.Stop();
            await vid_capture.Term();

            if (assert_message != "") {
                TST_ASSERT_FAIL_FATAL(assert_message);
            }
        }
    }

    async TST_CheckSetAudioVolume()
    {
        TST_STEP("check audio mute");
        await this.player.setVolume(0);
        await this.VerifyPlayback(10,1);
        const currentVol = await this.player.getVolume();
        console.log("volume is " + currentVol);
        TST_ASSERT(currentVol == 0, "volume is not set properly");

        for (let i = 1; i <= 4; i++) {
            const setVol = i * 25;
            TST_STEP("check audio volume: " + setVol);
            await this.player.setVolume(setVol);
            await this.VerifyPlayback(5,1);
            const currentVol = await this.player.getVolume();
            console.log("volume is " + currentVol);
            TST_ASSERT(currentVol == setVol, "volume is not set properly");
        }
    }

    async TST_CheckAvailableAudioTracks()
    {
        var availableAudioTracks = [];
        console.log("call getAvailableAudioTracks");
        await this.TST_GetAvailableAudioTracks(availableAudioTracks);
        var readAudioTrackListString = JSON.stringify(availableAudioTracks);
        var expectedAudioTrackListString = JSON.stringify(audioTracks);
        console.log("readAudioTrackListString: " + readAudioTrackListString);
        console.log("expectedAudioTrackListString" + expectedAudioTrackListString);
        TST_ASSERT(readAudioTrackListString == expectedAudioTrackListString, "audio tracks did not match");
        console.log("audio tracks matched");
    }
     // Set the audio track language from available audio track list.
    async TST_CheckSetAudioTrack(language, codec, rendition, availability) {
        console.log("Invoked set audio track " + language);
        var audTrackInfo = {};
        audTrackInfo.language = language;
        audTrackInfo.codec = codec;
        audTrackInfo.rendition = rendition;
        audTrackInfo.availability = availability;
        var availTracks = [];
        await this.TST_GetAvailableAudioTracks(availTracks);
        var matchingAudTrack = null;
        console.log("availTracks length " + availTracks.length);

        for (const availTrack of availTracks) {
            console.log("Checking track: " + JSON.stringify(availTrack));
            if (compareTracks(availTrack, audTrackInfo)) {
                matchingAudTrack = availTrack;
                break;
            }
        }

        if (matchingAudTrack !== null)
        {
            await this.TST_SetAudioTrack(matchingAudTrack.language,
                                                      matchingAudTrack.codec,
                                                      matchingAudTrack.rendition,
                                                      matchingAudTrack.availability);
        }
        else
        {
            console.log("No audio track found for the specified language:" + language);
        }

        const obtainedTrackIndex = await this.TST_GetAudioTrack();
        console.log("obtainedTrackIndex " + obtainedTrackIndex);
        let index = getIndexByMatchingTrack(availTracks, matchingAudTrack);
        console.log("index: " + index);
        TST_ASSERT(index == obtainedTrackIndex,"Audio track index obtained does not match the index set");

        function compareTracks(availTrack, audTrackInfo) {
            return (
                availTrack.language.toString() == audTrackInfo.language.toString() &&
                availTrack.codec.toString() == audTrackInfo.codec.toString() &&
                availTrack.rendition.toString() == audTrackInfo.rendition.toString() &&
                availTrack.availability.toString() == audTrackInfo.availability.toString()
            );
        }

        function getIndexByMatchingTrack(availTracks, matchingAudTrack) {
            if (matchingAudTrack !== null) {
                return availTracks.findIndex((track) => compareTracks(track, matchingAudTrack));
            }
            return -1;
        }
    }
}



