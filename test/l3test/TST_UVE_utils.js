// Main test asset URL (VOD)
var mainContentUrl = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/generated/main.mpd";

// Main test asset URL (Live)
var simlinearBaseUrl = "http://<host_ip>:<simlinear_port>"; // Placeholder to be replaced with host device IP
var simlinearContentUrl = simlinearBaseUrl + "/30003/88889531/hls/master.m3u8"

var adUrl = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/ads/Ad-generated-30s/ad-generated-30s.mpd"

var TST_fail_count = 0;
var TST_current_step = "";

// Function to get URL parameters
function getUrlParams() {
    const params = new URLSearchParams(window.location.search);
    return {
        DONT_END_TESTSUITE_ON_FAILURE: parseInt(params.get('DONT_END_TESTSUITE_ON_FAILURE'), 10) || 0
    };
}

// Extract variables
const vars = getUrlParams();

// Log a L3 step message to the console
function L3_CI_LOG(log_type, message){
    console.log("L3_LOG: " + "[" + log_type + "] " + message);
}

// Signal the start of the Test to the log (Specific string monitored by CI)
// Updates the running test number to the screen
function TST_START(message){
    TST_fail_count = 0;
    TST_current_step = "Test Start";
    L3_CI_LOG("TST_START", message)

    console.log("CI_CHECKPOINT TEST RUNNING");
    document.getElementById("testData").innerHTML = message;
    console.log("TST_START: " + message);
}

// Signal the End of the Test to the log (Specific string monitored by CI)
function TST_END(){

    L3_CI_LOG("TST_END", "")

    if (TST_fail_count == 0){
        document.getElementById("infoData").innerHTML = "TEST PASSED";
        console.log("TST_END: TEST PASSED");
        console.log("CI_CHECKPOINT TEST PASSED");
    }
    else{
        document.getElementById("infoData").innerHTML = "TEST FAILED: " + TST_current_step;
        console.log("TST_END: TEST FAILED: " + TST_current_step);
        console.log("CI_CHECKPOINT TEST FAILED");

        // Stop Execution of the test
        throw new Error("Stop Test");
    }
}

// Update the screen OSD and log the test step message
function TST_STEP(message){
    document.getElementById("infoData").innerHTML = "TST_STEP: " + message;
    console.log("TST_STEP: " + message);
    TST_current_step=message;
}

// Check condition - on failure Log the message, and throw an error to terminate the test.
// (Specific string monitored by CI)
function TST_ASSERT(condition, message) {

    if (!condition) {
        TST_fail_count += 1;
        document.getElementById("infoData").innerHTML = "ASSERT FAILED: (" + TST_current_step + ") " + message;
        console.log("TST_ASSERT: ASSERT FAILED: (" + TST_current_step + ") " + message);
        L3_CI_LOG("TST_STEP", TST_current_step + ": FAIL {" + message + "}");

        if(!vars.DONT_END_TESTSUITE_ON_FAILURE == 1) {

            // End the test and Exit
            TST_END();

        }

    }
    else {
        L3_CI_LOG("TST_STEP", TST_current_step + ": PASS");
    }

}

// Assert a specific failure
function TST_ASSERT_FAIL_FATAL(message) {
    TST_ASSERT(0, message);
}

// Perform a sleeep
async function TST_SLEEP(timeout) {
    console.log("TST_SLEEP: " + timeout);
    await new Promise(resolve => setTimeout(resolve, (timeout * 1000)));
}

// Catch all errors to prevent un-nescessary additions to the log file.
window.addEventListener("error", (event) => {
    event.preventDefault();
    return false;
});

// Catch all listener for unhandled rejections
window.addEventListener("unhandledrejection", (event) => {
    event.preventDefault();

    if (TST_fail_count == 0){

        console.log("TST_ASSERT: ASSERT FAILED: (" + TST_current_step + ") " + event.reason);
        document.getElementById("infoData").innerHTML = "TEST FAILED: " + TST_current_step;
        console.log("TST_END: TEST FAILED: " + TST_current_step);
        console.log("CI_CHECKPOINT TEST FAILED");
    }

    throw new Error('Exit App');
});


// ****************************** AAMP UVE JS ************************************
var aamp_State = { "eSTATE_IDLE":0,
                   "eSTATE_INITIALIZING":1,
                   "eSTATE_INITIALIZED":2,
                   "eSTATE_PREPARING":3,
                   "eSTATE_PREPARED":4,
                   "eSTATE_BUFFERING":5,
                   "eSTATE_PAUSED":6,
                   "eSTATE_SEEKING":7,
                   "eSTATE_PLAYING":8,
                   "eSTATE_STOPPING":9,
                   "eSTATE_STOPPED":10,
                   "eSTATE_COMPLETE":11,
                   "eSTATE_ERROR":12,
                   "eSTATE_RELEASED":13,
                   "eSTATE_BLOCKED":14};
var aamp_timeout = 10000;


// AAMP Player class
class AAMPPlayer {

    // Create an AAMP media player instance and register some listeners for monitoring purposes
    constructor(player_name) {
        this.player = new AAMPMediaPlayer();

        // Listener to log progress events
        function playbackProgressUpdate(event) {
            console.log(player_name + ": AAMP_EVENT_PROGRESS: " + JSON.stringify(event));
        }


        // Listener to log state changed
        function stateChangedEvent(event) {
            console.log(player_name + ": AAMP_STATE_CHANGED: " + JSON.stringify(event));
        }


        // Listener to log speed changed
        function speedChangedEvent(event) {
            console.log(player_name + ": AAMP_SPEED_CHANGED: " + JSON.stringify(event));
        }

        // Listener to log Text Track changed
        function textTracksChanged(event) {
            console.log(player_name + ": AAMP_TEXT_TRACKS_CHANGED: " + JSON.stringify(event));
        }

        function playlistIndexed(event) {
            console.log(player_name + ": AAMP_EVENT_PLAYLIST_INDEXED: " + JSON.stringify(event));
        }

        function anomalyReport(event) {
            console.log(player_name + ": AAMP_EVENT_REPORT_ANOMALY: " + JSON.stringify(event));
        }

        function mediaMetadata(event) {
            console.log(player_name + ": AAMP_EVENT_MEDIA_METADATA: " + JSON.stringify(event));
        }

        function placementStart(event) {
            console.log(player_name + ": AAMP_EVENT_AD_PLACEMENT_START: " +JSON.stringify(event));
        }

        function placementProgress(event) {
            console.log(player_name + ": AAMP_EVENT_AD_PLACEMENT_PROGRESS: " +JSON.stringify(event));
        }

        function placementEnd(event) {
            console.log(player_name + ": AAMP_EVENT_AD_PLACEMENT_END: " +JSON.stringify(event));
        }

        function reservationStart(event) {
            console.log(player_name + ": AAMP_EVENT_AD_RESERVATION_START: " +JSON.stringify(event));
        }

        function reservationEnd(event) {
            console.log(player_name + ": AAMP_EVENT_AD_RESERVATION_END: " +JSON.stringify(event));
        }

        function bitrateChanged(event) {
            console.log(player_name + ": AAMP_EVENT_BITRATE_CHANGED: " + JSON.stringify(event));
        }

        function seeked(event) {
            console.log(player_name + ": AAMP_EVENT_SEEKED: " + JSON.stringify(event));
        }

        function contentProtectionDataUpdate(event) {
            console.log(player_name + ": AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE: " + JSON.stringify(event));
        }

        function playbackStarted(event) {
            console.log(player_name + ": AAMP_EVENT_TUNED: " + JSON.stringify(event));
        }

        function playbackCompleted(event) {
            console.log(player_name + ": AAMP_EVENT_EOS: " + JSON.stringify(event));
        }

        function playbackFailed(event) {
            console.log(player_name + ": AAMP_EVENT_TUNE_FAILED: " + JSON.stringify(event));
        }

        function decoderAvailable(event) {
            console.log(player_name + ": AAMP_EVENT_CC_HANDLE_RECEIVED: " +JSON.stringify(event));
        }

        function subscribedTagNotifier(event) {
            console.log(player_name + " AAMP_EVENT_TIMED_METADATA " + JSON.stringify(event));
        }

        function drmMetadata(event) {
            console.log(player_name + ": AAMP_EVENT_DRM_METADATA: " + JSON.stringify(event));
        }

        function audioTracksChanged(event) {
            console.log(player_name + ": AAMP_EVENT_AUDIO_TRACKS_CHANGED: " + JSON.stringify(event));
        }

        function durationChanged(event) {
            console.log(player_name +": AAMP_EVENT_DURATION_CHANGED: " + JSON.stringify(event));
        }

        function enteringLive(event) {
            console.log(player_name + ": AAMP_EVENT_ENTERING_LIVE: " + JSON.stringify(event));
        }

        function id3Metadata(event) {
            console.log(player_name + ": AAMP_EVENT_ID3_METADATA: " + JSON.stringify(event));
        }

        function contentGap(event) {
            console.log(player_name + ": AAMP_EVENT_CONTENT_GAP: " + JSON.stringify(event));
        }

        function bulkTimedMetadata(event) {
            console.log(player_name + ": AAMP_EVENT_BULK_TIMED_METADATA: " + JSON.stringify(event));
        }

        function tuneProfiling(event) {
            console.log(player_name + ": AAMP_EVENT_TUNE_PROFILING: " + JSON.stringify(event));
        }

        function httpResponseHeader(event) {
            console.log(player_name + ": AAMP_EVENT_HTTP_RESPONSE_HEADER: " + JSON.stringify(event));
        }

        function drmMessage(event) {
            console.log(player_name + ": AAMP_EVENT_DRM_MESSAGE: " + JSON.stringify(event));
        }

        function metricsData(event) {
            console.log(player_name +": AAMP_EVENT_REPORT_METRICS_DATA: " + JSON.stringify(event));
        }

        function bufferingChanged(event) {
            console.log(player_name + ": AAMP_EVENT_BUFFERING_CHANGED: " + JSON.stringify(event));
        }

        this.player.addEventListener("playbackProgressUpdate", playbackProgressUpdate);
        this.player.addEventListener("playbackStateChanged", stateChangedEvent);
        this.player.addEventListener("playbackSpeedChanged", speedChangedEvent);
        this.player.addEventListener("textTracksChanged", textTracksChanged);
        this.player.addEventListener("playlistIndexed", playlistIndexed);
        this.player.addEventListener("anomalyReport", anomalyReport);
        this.player.addEventListener("mediaMetadata",mediaMetadata);
        this.player.addEventListener("placementStart",placementStart);
        this.player.addEventListener("placementProgress",placementProgress);
        this.player.addEventListener("placementEnd",placementEnd);
        this.player.addEventListener("reservationStart",reservationStart);
        this.player.addEventListener("reservationEnd",reservationEnd);
        this.player.addEventListener("bitrateChanged",bitrateChanged);
        this.player.addEventListener("seeked",seeked);
        this.player.addEventListener("contentProtectionDataUpdate", contentProtectionDataUpdate);
        this.player.addEventListener("playbackStarted",playbackStarted);
        this.player.addEventListener("playbackCompleted", playbackCompleted);
        this.player.addEventListener("playbackFailed",playbackFailed);
        this.player.addEventListener("decoderAvailable",decoderAvailable);
        this.player.addEventListener("timedMetadata",subscribedTagNotifier);
        this.player.addEventListener("drmMetadata",drmMetadata);
        this.player.addEventListener("audioTracksChanged",audioTracksChanged);
        this.player.addEventListener("durationChanged",durationChanged);
        this.player.addEventListener("enteringLive",enteringLive);
        this.player.addEventListener("id3Metadata",id3Metadata);
        this.player.addEventListener("contentGap",contentGap);
        this.player.addEventListener("bulkTimedMetadata",bulkTimedMetadata);
        this.player.addEventListener("tuneProfiling",tuneProfiling);
        this.player.addEventListener("httpResponseHeader",httpResponseHeader);
        this.player.addEventListener("drmMessage",drmMessage);
        this.player.addEventListener("metricsData",metricsData);
        this.player.addEventListener("bufferingChanged",bufferingChanged);


        this.player_name = player_name;
        this.url = "";
    }


    // Destroy the AAMP media player instance
    destroy() {
        this.player.release();
        this.player = null;
    }


    // Blocking function which waits for a listener callback before returning.
    waitForEventWithTimeout(emitter, eventName, timeout, action_fn) {
        return new Promise((resolve, reject) => {
            let timer;

            function listener(data) {
                console.log("Listener (" + eventName + "): " + JSON.stringify(data));
                clearTimeout(timer);
                emitter.removeEventListener(eventName, listener);
                resolve(data);
            }

            emitter.addEventListener(eventName, listener);

            timer = setTimeout(() => {
                console.log("TIMEOUT: waitForEventWithTimeout");
                emitter.removeEventListener(eventName, listener);
                reject(new Error("TIMEOUT: waiting for " + eventName));
            }, timeout);

            if (action_fn !== null){
                console.log("Call action function");
                action_fn();
            }
        });
    }


    // Waits for a number of progress intervals and checks the timings of them..
    async VerifyPlayback(wait_s, speed, waitForProgress = true)
    {
        console.log("aamp_VerifyPlayback ENTER: Wait "+ wait_s + ", Speed " + speed + ", waitForProgress " + waitForProgress);

        if (waitForProgress)
        {
            console.log("aamp_VerifyPlayback START");

            let notifications = 0;
            let wait_ms = (wait_s * 1000)
            let start_ms = Date.now();
            let diff_ms = 0
            while (diff_ms <= wait_ms)
            {
                var progress = await this.waitForEventWithTimeout(this.player, 'playbackProgressUpdate', aamp_timeout, null).catch(e => {
                    TST_ASSERT_FAIL_FATAL(e);
                });
                console.log("aamp_VerifyPlayback ("+ JSON.stringify(progress) +")");
                TST_ASSERT((speed == progress.currentPlayRate), "Unexpected Playback Rate")

                // Increase notification count
                notifications += 1;

                let end_ms = Date.now();
                diff_ms = end_ms - start_ms;
                console.log("aamp_VerifyPlayback: diff_ms "+ diff_ms);
            }

            console.log("aamp_VerifyPlayback END: diff_ms "+ diff_ms + ", Notifications " + notifications);
        }
        else
        {
            await new Promise(resolve => setTimeout(resolve, (wait_s * 1000)));
        }
        console.log("aamp_VerifyPlayback EXIT");
    }

    async VerifyPositionProgress(wait_s, speed, direction)
    {
        // Waits for a number of progress intervals and checks the timings of them..
        console.log("VerifyPositionProgress ENTER: Wait "+ wait_s + ", Speed " + speed + "direction " + direction);

        let notifications = 0;
        let wait_ms = (wait_s * 1000)
        let start_ms = Date.now();
        let diff_ms = 0
        let previousPosition = 0;
        let firstSampleTaken = false;
        while (diff_ms <= wait_ms)
        {
            var progress = await this.waitForEventWithTimeout(this.player, 'playbackProgressUpdate', aamp_timeout, null).catch(e => {
                TST_ASSERT_FAIL_FATAL(e);
            });
            console.log("VerifyPositionProgress ("+ JSON.stringify(progress) +")");
            TST_ASSERT((speed == progress.currentPlayRate), "Unexpected Playback Rate")
            console.log("VerifyPositionProgress: previousPosition " + previousPosition + " progress.positionMiliseconds " + progress.positionMiliseconds);
            if(firstSampleTaken && (direction == "fw"))
            {
                TST_ASSERT((previousPosition < progress.positionMiliseconds),"Position found backwards while going in forward direction" );
            }
            else if(firstSampleTaken && (direction == "rw"))
            {
                TST_ASSERT((previousPosition > progress.positionMiliseconds),"Position found forward while going in backward direction" );
            }
            else
            {
                firstSampleTaken = true;
            }
            previousPosition = progress.positionMiliseconds;
            notifications += 1;

            let end_ms = Date.now();
            diff_ms = end_ms - start_ms;
            console.log("VerifyPositionProgress: diff_ms "+ diff_ms);
        }

        console.log("VerifyPositionProgress END: diff_ms "+ diff_ms + ", Notifications " + notifications);

    }

    // Plays a given URL waiting for AAMP to signal the eSTATE_PLAYING state has been reached
    async Load(url, autostart = true, waitForState = true)
    {
        console.log("aamp_Load ENTER: url " + url + ", autostart " + autostart + ", waitForState " + waitForState);

        if (waitForState)
        {
            var i = 0;
            var current_state = this.player.getCurrentState();
            var expected_state = aamp_State.eSTATE_PLAYING;

            if (!autostart)
            {
                // Events which are not started yet will move to the prepared state then stop.
                var expected_state = aamp_State.eSTATE_PREPARED;
            }

            console.log("aamp_Load START: (Current State:" + current_state + ", Expected State: " + expected_state +")");

            while (current_state != expected_state)
            {
                var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.load(url, autostart) : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Load (Current State:" + current_state + ", Expected State: " + expected_state +")");
                i += 1;

                // Multiple listeners can be called - recheck state before looping again
                var new_state = this.player.getCurrentState();
                if (new_state != current_state)
                {
                    current_state = new_state;
                    console.log("aamp_Load (State Updated: Current State:" + current_state +")");
                    console.log("aamp_Load (Current State:" + current_state + ", Expected State: " + expected_state +")");
                }
            }

            console.log("aamp_Load END: (Current State:" + current_state + ", Expected State: " + expected_state +")");
        }
        else
        {
            this.player.load(url, autostart);
        }

        console.log("aamp_Load EXIT");
    }


    // Sets the playback rate waiting for AAMP to signal that the playback speed has changed
    async SetRate(rate, waitForRateChange = true)
    {
        console.log("aamp_SetRate ENTER: rate " + rate +", waitForRateChange " + waitForRateChange);

        if (waitForRateChange)
        {
            var i = 0;
            var current_rate = this.player.getPlaybackRate();
            console.log("aamp_SetRate START: (Current Rate:" + current_rate + ", Expected Rate: " + rate +")");

            while (current_rate != rate)
            {
                var speedChanged = await this.waitForEventWithTimeout(this.player, 'playbackSpeedChanged', aamp_timeout, (i == 0) ? () => this.player.setPlaybackRate(rate) : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_rate = speedChanged.speed;
                console.log("aamp_SetRate (Current Rate:" + current_rate + ", Expected Rate: " + rate +")");
                i += 1;
            }

            console.log("aamp_SetRate END: (Current Rate:" + current_rate + ", Expected Rate: " + rate +")");
        }
        else
        {
            this.player.setPlaybackRate(rate);
        }

        console.log("aamp_SetRate EXIT");
    }


    // Seeks to the given position waiting for AAMP to signal the eSTATE_SEEKING has been reached and then waiting
    // for AAMP to signal its return to the original state
    async Seek(position, waitForState = true)
    {
        console.log("aamp_Seek: ENTER: position " + position + ", waitForState " + waitForState);

        if (waitForState)
        {
            var i = 0;
            var current_state = this.player.getCurrentState();
            var initial_state = current_state;
            console.log("aamp_Seek START: (Current State:" + current_state + ", Expected Position: " + position +")");

            while (current_state != aamp_State.eSTATE_SEEKING)
            {
                var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.seek(position) : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Seek (Current State:" + current_state + ", Expected State: " + aamp_State.eSTATE_SEEKING +")");
                i += 1;
            }

            console.log("aamp_Seek SEEKING - Wait for Initial: (Current State:" + current_state + ", Expected State: " + initial_state +")");

            while (current_state != initial_state)
            {
                stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Seek (Current State:" + current_state + ", Expected State: " + initial_state +")");
            }

            position = this.player.getCurrentPosition();
            console.log("aamp_Seek END: (Current State:" + current_state + ", New Position: " + position +")");
        }
        else
        {
            this.player.seek(position);
        }

        console.log("aamp_Seek EXIT");
    }


    // Stops the playback waiting for AAMP to signal that the eSTATE_IDLE sate has been reached
    async Stop(waitForState = true)
    {
        console.log("aamp_Stop ENTER: waitForState " + waitForState);

        if (waitForState)
        {
            var i = 0;
            var current_state = this.player.getCurrentState();

            while (current_state != eSTATE_STOPPING)
            {
                    var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.stop() : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                    current_state = stateChanged.state;
                    console.log("aamp_Stop (Current State:" + current_state +")");
                    i += 1;
            }

            var expected_state = aamp_State.eSTATE_RELEASED;
            console.log("aamp_Stop START: (Current State:" + current_state + ", Expected State: " + expected_state +")");

            while (current_state != expected_state)
            {
                var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.stop() : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Stop (Current State:" + current_state + ", Expected State: " + expected_state +")");
                i += 1;
            }

            console.log("aamp_Stop END: (Current State:" + current_state + ", Expected State: " + expected_state +")");
        }
        else
        {
            this.player.stop();
        }

        console.log("aamp_Stop EXIT");
    }

    // Starts the playback waits for AAMP to signal that the eSTATE_PLAYING has been reached
    async Play(waitForState = true)
    {
        console.log("aamp_Play ENTER: waitForState " + waitForState);

        if (waitForState)
        {
            var i = 0;
            var current_state = this.player.getCurrentState();
            var expected_state = aamp_State.eSTATE_PLAYING;
            console.log("aamp_Play START: (Current State:" + current_state + ", Expected State: " + expected_state +")");

            while (current_state != expected_state)
            {
                var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.play() : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Play (Current State:" + current_state + ", Expected State: " + expected_state +")");
                i += 1;
            }

            console.log("aamp_Play END: (Current State:" + current_state + ", Expected State: " + expected_state +")");
        }
        else
        {
            this.player.play();
        }

        console.log("aamp_Play EXIT");
    }

    // Returns the available audio tracks information in the content.
    async TST_GetAvailableAudioTracks(trackInfoList) {
        console.log("calling getAvailableAudioTracks()");
        var avlAudioTracks = await this.player.getAvailableAudioTracks();
        if (avlAudioTracks != undefined) {
            var audioTrackList = JSON.parse(avlAudioTracks);
            for (var i in audioTrackList) {
                var trackInfo = {};
                trackInfo.language = audioTrackList[i].language;
                trackInfo.codec = audioTrackList[i].codec;
                trackInfo.rendition = audioTrackList[i].rendition;
                trackInfo.availability = audioTrackList[i].availability;
                trackInfo.index = i;
                console.log("trackInfo.index: " + trackInfo.index);
                console.log("trackInfo.language: " + trackInfo.language);
                console.log("trackInfo.codec: " + trackInfo.codec);
                console.log("trackInfo.rendition: " + trackInfo.rendition);
                console.log("trackInfo.availability: " + trackInfo.availability);
                trackInfoList.push(trackInfo);
            }
            return trackInfoList;
        } else {
            console.log("avlAudioTracks is empty");
            return []; // Return an empty array if avlAudioTracks is empty
        }
    }

    // Set the audio track from available audio track list.
    async TST_SetAudioTrack(language, codec, rendition, availability)
    {
        console.log("Invoked set audio track " + language);
        var audioTracks = {};
        audioTracks.language = language;
        audioTracks.codec = codec;
        audioTracks.rendition = rendition;
        audioTracks.availability = availability;
        var availableAudioTracks = this.player.getAvailableAudioTracks();
        var avlTracks = JSON.parse(availableAudioTracks);
        var matchingAudioTrack = null;
        console.log("avlTracks length " + avlTracks.length);
        for (const avlTrack of avlTracks)
        {
            console.log("Checking track: " + JSON.stringify(avlTrack));
            if((avlTrack.language.toString() == audioTracks.language.toString()) &&
                (avlTrack.codec.toString() == audioTracks.codec.toString()) &&
                (avlTrack.rendition.toString() == audioTracks.rendition.toString()) &&
                (avlTrack.availability.toString() == audioTracks.availability.toString()))
            {
                matchingAudioTrack = avlTrack;
                break;
            }
        }
        if (matchingAudioTrack !== null)
        {
            await this.player.setAudioTrack(matchingAudioTrack);
        }
        else
        {
            console.log("No audio track found for the specified language:" + language);
        }
    }

    // Returns the index of current audio track in available audio track list.
    async TST_GetAudioTrack()
    {
        const currentAudioTrack = this.player.getAudioTrack();
        console.log("Current audio track: " + currentAudioTrack);
        return currentAudioTrack;
    }

//Returns the audio track info
    async TST_GetAudioTrackInfo() {
        console.log("invoked getAudioTrackInfo");
        var Info = await this.player.getAudioTrackInfo();
        var InfoJson = JSON.parse(Info);
        console.log("TrackInfo: " + JSON.stringify(InfoJson));
        return InfoJson;
    }

//Returns the properties of Preferred Audio
    async TST_GetPreferredAudioProperties() {
        console.log("invoked getPreferredAudioProperties");
        var properties = await this.player.getPreferredAudioProperties();
        var AudProperties = JSON.parse(properties);
        console.log("Audio Properties " + JSON.stringify(AudProperties));
        return AudProperties;
    }

    //Returns current duration of content in seconds.
    async TST_GetDurationSec() {
        console.log("invoked getDurationSec");
        var duration = await this.player.getDurationSec();
        console.log("duration " + duration);
        if(duration < 0)
        {
            console.log("duration of the content is less than 0");
        }
        return duration;
    }

    // Returns the available video bitrates
    async TST_GetVideoBitrates() {
        console.log("calling getVideoBitrates()");
        var avlBitrates = await this.player.getVideoBitrates();
        console.log("avlBitrates : " + avlBitrates);
        return avlBitrates;
    }

// add the http header
    async TST_AddCustomHTTPHeader(headerName, headerValue, isLicenseRequest) {
        console.log("invoked addCustomHTTPHeader");
        console.log("header_name: " + headerName, "header_value: " + headerValue, "isLicenseRequest: " + isLicenseRequest);
        try {
            await this.player.addCustomHTTPHeader(headerName, headerValue, isLicenseRequest);
            console.log("header added successfully");
        } catch (error) {
            console.error("Error adding custom HTTP header:", error);
        }
    }

// get the available video tracks
    async TST_GetAvailableVideoTracks(trackList)
    {
        console.log("invoked getAvailableVideoTracks");
        var avlVideoTracks = await this.player.getAvailableVideoTracks();
        if(avlVideoTracks != undefined)
        {
            var videoTrackList = JSON.parse(avlVideoTracks);
            console.log("videoTracks  " + JSON.stringify(videoTrackList));
            for (var i in videoTrackList)
            {
                var track = {};
                track.bandwidth = videoTrackList[i].bandwidth;
                track.width = videoTrackList[i].width;
                track.height = videoTrackList[i].height;
                track.framerate = videoTrackList[i].framerate;
                track.enabled = videoTrackList[i].enabled;
                track.index=i;
                console.log("track.bandwidth: " +track.bandwidth);
                console.log("track.width: " +track.width);
                console.log("track.height: " +track.height);
                console.log("track.framerate: " +track.framerate);
                console.log("track.enabled: " +track.enabled);
                trackList.push(track);
            }
            return trackList;
        }
        else
        {
            console.log("avlVideoTracks are empty");
        }
    }

// remove the http header
    async TST_RemoveCustomHTTPHeader(headerName) {
        console.log("invoked removeCustomHTTPHeader");
        await this.player.removeCustomHTTPHeader(headerName);
        console.log("header removed successfully");
    }

// sets the video zoom
    async TST_SetVideoZoom(videoZoom) {
        console.log("invoked setVideoZoom");
        console.log("videoZoom :" + videoZoom);
        if(videoZoom == "none")
        {
            await this.player.setVideoZoom(videoZoom);
            console.log("video zoom mode disabled");
        }
        else
        {
            await this.player.setVideoZoom(videoZoom);
            console.log("video zoom mode enabled");
        }
    }

// sets the rectangle coordinates
    async TST_SetVideoRect( x, y, w, h ) {
        console.log("invoked setVideoRect");
        console.log("left position :" + x);
        console.log("top position :" + y);
        console.log("video width :" + w);
        console.log("video height :" + h);
        await this.player.setVideoRect(x, y, w, h);
        console.log(" video rectangle set successfully");
    }

// get current video bitrate
    async TST_GetCurrentVideoBitrate()
    {
        console.log("getCurrentVideoBitrate");
        var currentVideoBitrate = await this.player.getCurrentVideoBitrate();
        console.log("currentVideoBitrate " + currentVideoBitrate);
        return currentVideoBitrate;
    }

// sets the video track
    async TST_SetVideoTracks(bitratelist) {
        console.log("invoked setVideoTrack");
        console.log("bitrates are " +bitratelist);
        await this.player.setVideoTracks(...bitratelist);
    }

    // Returns the available audio tracks information in the content.
    async getAvailableThumbnailTracks(InfoList)
    {
        var avlThumbnailTracks = await this.player.getAvailableThumbnailTracks();
        if (avlThumbnailTracks != undefined)
        {
            var thumbnailTracksList = JSON.parse(avlThumbnailTracks);
            for (var i in thumbnailTracksList)
            {
                var ThumbnailInfo = {RESOLUTION: thumbnailTracksList[i].RESOLUTION,
                BANDWIDTH: thumbnailTracksList[i].BANDWIDTH};
                InfoList.push(ThumbnailInfo);
             }
             console.log(JSON.stringify(InfoList));
             return InfoList;
        }
        else
        {
            console.log("AvailableThumbnailTracks are empty");
        }
    }

    //gets the manifest data.
    async TST_GetManifest() {
        console.log("invoked getManifest");
        var content = await this.player.getManifest();
        var manifesturl = JSON.stringify(content);
        console.log("manifest " + manifesturl);
        return manifesturl;
    }

    // sets the thumbnail track based on the index provided.
    async TST_SetThumbnailTrack(index) {
        console.log("calling setThumbnailTrack");
        var val = await this.player.setThumbnailTrack(index);
        console.log("index " + index);
        console.log("val " + val);
    }

    // gets the thumbnail information that is specified in between start and end position.
    async TST_GetThumbnail(startPosition,endPosition) {
        console.log("calling getThumbnail");
        var get = await this.player.getThumbnail(startPosition,endPosition);
        console.log("startposition " + startPosition);
        console.log("endposition " +endPosition);
        console.log("get " +JSON.stringify( get ));
    }

    // gets the available audio bitrates
    async TST_GetAudioBitrates() {
        console.log("calling getAudioBitrates()");
        var avlAudBitrates = await this.player.getAudioBitrates();
        console.log("avlAudBitrates : " + avlAudBitrates);
        return avlAudBitrates;
    }


    // Subscribe to specific tags / metadata in manifest
    async TST_SetSubscribedTags(tagNames){
        console.log("invoked tagNames");
        console.log("tagNames: " + tagNames);
        await this.player.setSubscribedTags(tagNames);
        console.log("set successful");
    }


    // sets the video bitrate
    async TST_SetVideoBitrate(bitrate) {
        console.log("Invoked setVideoBitrate: " + bitrate);
        if (bitrate == 0) {
            await this.player.setVideoBitrate(0);
            console.log("ABR enabled");
        } else {
            var availableBitrates = await this.player.getVideoBitrates();
            if (availableBitrates.includes(bitrate)) {
                var currentVideoBitrate = await this.player.getCurrentVideoBitrate();
                console.log("currentVideoBitrate " + currentVideoBitrate);
                await this.player.setVideoBitrate(bitrate);
                console.log("Video bitrate set to: " + bitrate);
            } else {
                console.log("Bitrate is not available " + availableBitrates.toString());
            }
        }
    }

    // gets the current video bitrate
    async TST_GetCurrentVideoBitrate()
    {
        console.log("getCurrentVideoBitrate");
        var currentVideoBitrate = await this.player.getCurrentVideoBitrate();
        console.log("currentVideoBitrate " + currentVideoBitrate);
        return currentVideoBitrate;
    }

    //Subscribe http response headers from manifest download
    async TST_SubscribeResponseHeaders(headers) {
        console.log("aamp_subscribeResponseHeaders ENTER: headers " + headers );
        this.player.subscribeResponseHeaders(headers);
        console.log("aamp_subscribeResponseHeaders EXIT");
    }

    // Pauses the playback at a given time and waits for AAMP to signal that the eSTATE_PAUSED has been reached
    async Pause(position, waitForState = true)
    {
        console.log("aamp_Pause ENTER: position " + position + ", waitForState " + waitForState);

        if (waitForState)
        {
            var i = 0;
            var current_state = this.player.getCurrentState();
            var expected_state = aamp_State.eSTATE_PAUSED;
            console.log("aamp_Pause START: (Current State:" + current_state + ", Expected State: " + expected_state +")");

            while (current_state != expected_state)
            {
                var stateChanged = await this.waitForEventWithTimeout(this.player, 'playbackStateChanged', aamp_timeout, (i == 0) ? () => this.player.pause(position) : null).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
                current_state = stateChanged.state;
                console.log("aamp_Pause (Current State:" + current_state + ", Expected State: " + expected_state +")");
                i += 1;
            }
            console.log("aamp_Pause END: (Current State:" + current_state + ", Expected State: " + expected_state +")");
        }
        else
        {
            this.player.pause(position);
        }

        console.log("aamp_Pause EXIT");
    }

    // Changes the Text Track and waits for AAMP to signal that the Text Track has been reached
    async SetTextTrack(trackIndex, waitForEvent = true)
    {
        console.log("aamp_SetTextTrack ENTER: trackIndex " + trackIndex + ", waitForEvent " + waitForEvent);

        if (waitForEvent)
        {
            await this.waitForEventWithTimeout(this.player, 'textTracksChanged', aamp_timeout, () => this.player.setTextTrack(trackIndex)).catch(e => { TST_ASSERT_FAIL_FATAL(e) });
        }
        else
        {
            this.player.setTextTrack(trackIndex);
        }

        console.log("aamp_SetTextTrack EXIT");
    }
};
