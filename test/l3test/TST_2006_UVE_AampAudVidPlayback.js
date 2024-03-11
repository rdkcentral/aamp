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

const TST_2006_url = "https://cpetestutility.stb.r53.xcal.tv/multilang/main.m3u8";

class AAMPPlayer2 extends AAMPPlayer{
    constructor(player_name) {
        super(player_name);
    }
    async TST_CheckSetAudioVolume()
    {
        for (let i = 0; i <= 4; i++) {
            const setVol = i * 25;
            await this.player.setVolume(setVol);
            await this.VerifyPlayback(10,1);
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



