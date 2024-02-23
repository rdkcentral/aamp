/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

var controlObj = null;
var bitrateList = [];
var ccStatus = false;
var disableButtons = false;
var mainMenuToggle = true;
var configMenuToggle = false;
var debugMenuToggle = false;

var currentObjID = "";
const xreCCOptions1 = { textItalicized: false, textEdgeStyle:"none", textEdgeColor:"black", textSize: "large", windowFillColor: "black", fontStyle: "default", textForegroundColor: "black", windowFillOpacity: "transparent", textForegroundOpacity: "solid", textBackgroundColor: "white", textBackgroundOpacity:"solid", windowBorderEdgeStyle: "none", windowBorderEdgeColor: "blue", textUnderline: false };
const xreCCOptions2 = { textItalicized: true, textEdgeStyle:"none", textEdgeColor:"black", textSize: "small", windowFillColor: "black", fontStyle: "default", textForegroundColor: "blue", windowFillOpacity: "transparent", textForegroundOpacity: "solid", textBackgroundColor: "red", textBackgroundOpacity:"solid", windowBorderEdgeStyle: "none", windowBorderEdgeColor: "blue", textUnderline: true };
const ccOptions1 = {"penItalicized":false,"textEdgeStyle":"none","textEdgeColor":"black","penSize":"small","windowFillColor":"black","fontStyle":"default","textForegroundColor":"black","windowFillOpacity":"transparent","textForegroundOpacity":"solid","textBackgroundColor":"cyan","textBackgroundOpacity":"solid","windowBorderEdgeStyle":"none","windowBorderEdgeColor":"black","penUnderline":false};
const ccOptions2 = {"penItalicized":false,"textEdgeStyle":"none","textEdgeColor":"red","penSize":"large","windowFillColor":"black","fontStyle":"default","textForegroundColor":"red","windowFillOpacity":"transparent","textForegroundOpacity":"solid","textBackgroundColor":"black","textBackgroundOpacity":"solid","windowBorderEdgeStyle":"none","windowBorderEdgeColor":"red","penUnderline":false};
// create cc option object for xre receiver cc rendering
var xreCCOptions = {};

for(var option in ccOptions) {
    if( option === "penItalicized") {
        xreCCOptions.textItalicized = ccOptions[option];
    } else if ( option === "penSize") {
        xreCCOptions.textSize = ccOptions[option];
    } else if ( option === "penUnderline") {
        xreCCOptions.textUnderline = ccOptions[option];
    } else {
        xreCCOptions[option] = ccOptions[option];
    }
}

function playPause() {
    console.log("playPause");

    if (playerState === playerStatesEnum.idle) {
        //Play first video when clicking Play button first time
        document.getElementById("contentURL").innerHTML = "URL: " + urls[0].url;
        resetPlayer();
        resetUIOnNewAsset();
        loadUrl(urls[0], false);
    } else {
        // If it was a trick play operation
        if ( playbackSpeeds[playbackRateIndex] != 1 ) {
            // Change to normal speed
            playerObj.play();
        } else {
            if (playerState === playerStatesEnum.paused) {
                // Play the video
                playerObj.play();
            } else { // Pause the video
                playerObj.pause();
            }
        }
        playbackRateIndex = playbackSpeeds.indexOf(1);
    }
};

function mutePlayer() {
    if (mutedStatus === false) {
        // Mute
        playerObj.setVolume(0);
        mutedStatus = true;
        document.getElementById("muteIcon").src = "../icons/mute.png";
    } else {
        // Unmute
        playerObj.setVolume(100);
        mutedStatus = false;
        document.getElementById("muteIcon").src = "../icons/unMute.png";
    }
};

function resizeVideo() {
    if (fullScreenVideo === true) {
        // Scale to small Screen
        document.getElementById("scaleIcon").src = "../icons/expand.png";
        // Show debug Screen
        document.getElementById("debugScreen").style.display = "block";
        document.getElementById("innerNavBar").style.display = "block";
        debugMenuToggle = true;
        let w = screen.width/2; // 50% width
        let h = screen.height/2; // 50% height
        let x = 0; // align left
        let y = (screen.height/100)* 20; // place at 80% of screen height
        drawVideoRectHelper(x, y, w, h ); // place video using graphics plane coordinates
        fullScreenVideo = false;
    } else {
        // Scale to full Screen
        document.getElementById("scaleIcon").src = "../icons/compress.png";
        // Hide debug Screen
        document.getElementById("debugScreen").style.display = "none";
        document.getElementById("innerNavBar").style.display = "none";
        debugMenuToggle = false;
        let w = screen.width; // full width
        let h = screen.height; // full height
        drawVideoRectHelper(0, 0, w, h);
        fullScreenVideo = true;
    }
};

// helper function to set video position
function drawVideoRectHelper(x, y, w, h) {
	let video = document.getElementById("dummyVideo");
	video.style.left = x + "px";
	video.style.top = y + "px";
	video.style.width = w + "px";
	video.style.height = h + "px";
	playerObj.setVideoRect(x, y, w, h ); // place video using graphics plane coordinates
}


function toggleCC() {
    if (ccStatus === false) {
        // CC ON
        if(enableNativeCC) {
            playerObj.setClosedCaptionStatus(true);
            playerObj.setTextStyleOptions(JSON.stringify(ccOptions));
        } else if(typeof(XREReceiver) !== "undefined") {
            XREReceiver.onEvent("onClosedCaptions", { enable: true });
            XREReceiver.onEvent("onClosedCaptions", { setOptions: xreCCOptions});
        }
        ccStatus = true;
        document.getElementById("ccIcon").src = "../icons/closedCaptioning.png";
        document.getElementById('ccContent').innerHTML = "CC Enabled";    
    } else {
        // CC OFF
        if(enableNativeCC) {
            playerObj.setClosedCaptionStatus(false);
        } else if(typeof(XREReceiver) !== "undefined") {
            XREReceiver.onEvent("onClosedCaptions", { enable: false });
        }
        ccStatus = false;
        document.getElementById("ccIcon").src = "../icons/closedCaptioningDisabled.png";
        document.getElementById('ccContent').innerHTML = "CC Disabled";
    }
    document.getElementById('ccModal').style.display = "block";
    setTimeout(function(){  document.getElementById('ccModal').style.display = "none"; }, 2000);
};

function goToHome() {
    window.location.href = "../index.html";
}

function skipTime(tValue) {
    //if no video is loaded, this throws an exception
    try {
        var position = playerObj.getCurrentPosition();
        if (!isNaN(position)) {
            if(document.getElementById("seekCheck").checked) {
                // call old seek API
                playerObj.seek(position + tValue);
            } else {
                // call new seek API with support to seek with pause
                playerObj.pause();
                playerObj.seek(position + tValue, true);
            }
        }
    } catch (err) {
        console.log("Video content might not be loaded: " + err);
    }
}

function skipBackward() {
    skipTime(-300);
};

function skipForward() {
    skipTime(300);
};

function fastrwd() {
    var newSpeedIndex = playbackRateIndex - 1;
    if (newSpeedIndex < 0) {
        newSpeedIndex = 0;
    }
    if (newSpeedIndex !== playbackRateIndex) {
        console.log("Change speed from [" + playbackSpeeds[playbackRateIndex] + "] -> [" + playbackSpeeds[newSpeedIndex] + "]");
        playerObj.setPlaybackRate(playbackSpeeds[newSpeedIndex]);
    }
};

function fastfwd() {
    var newSpeedIndex = playbackRateIndex + 1;
    if (newSpeedIndex >= playbackSpeeds.length) {
        newSpeedIndex = playbackSpeeds.length - 1;
    }
    if (newSpeedIndex !== playbackRateIndex) {
        console.log("Change speed from [" + playbackSpeeds[playbackRateIndex] + "] -> [" + playbackSpeeds[newSpeedIndex] + "]");
        playerObj.setPlaybackRate(playbackSpeeds[newSpeedIndex]);
    }
};

//  load video file from select field
function getVideo(cache_only) {
    var fileURLContent = document.getElementById("videoURLs").value; // get select field
    if (fileURLContent != "") {
        var newFileURLContent = fileURLContent;
        document.getElementById("contentURL").innerHTML = "URL: " + fileURLContent;
        //get the selected index of the URL List
        var selectedURL = document.getElementById("videoURLs");
        var optionIndex = selectedURL.selectedIndex;
        //set the index to the selected field
        document.getElementById("videoURLs").selectedIndex = optionIndex;

        console.log(newFileURLContent);
        if(cache_only)
        {
	        for ( urlIndex = 0; urlIndex < urls.length; urlIndex++) {
	            if (newFileURLContent === urls[urlIndex].url) {
	                console.log("FOUND at index: " + urlIndex);
	                cacheStream(urls[urlIndex], false);
	                break;
	            }
	        }
        }
        else
        {
            resetPlayer();
            resetUIOnNewAsset();
            for ( urlIndex = 0; urlIndex < urls.length; urlIndex++) {
                if (newFileURLContent === urls[urlIndex].url) {
                    console.log("FOUND at index: " + urlIndex);
                    loadUrl(urls[urlIndex], false);
                    break;
                }
            }
        }
    } else {
        console.log("Enter a valid video URL");
    }
}

//function to Change the Audio Track
function changeAudioTrack() {
    var audString = (document.getElementById("audioTracks").value).replace("\"language\":", "\"languages\":");
    var audioTrackObj  = JSON.parse(audString); // get selected Audio track
    // fix the mismatch between languages name in the get&set apis
    var langListString = audioTrackObj["languages"];
    var langList = langListString.split(","); // if there are multiple languages
    audioTrackObj["languages"] = langList;
    console.log("Setting Audio track: " + JSON.stringify(audioTrackObj));
    playerObj.setPreferredAudioLanguage(JSON.stringify(audioTrackObj));
}

//function to Change the Closed Captioning Track
function changeCCTrack() {
    if (ccStatus === true) {
        //if CC is enabled
        var trackID =  document.getElementById("ccTracks").value; // get selected cc track
        if(enableNativeCC) {
            //Find trackIndex of CC track with language
            let tracks = JSON.parse(playerObj.getAvailableTextTracks(true));
            let trackIdx = tracks.findIndex(tr => { return tr['sub-type'] === "CLOSED-CAPTIONS" && tr.language === trackID; })
            console.log("Found trackIdx: " + trackIdx);
            playerObj.setTextTrack(trackIdx);
        } else if(typeof(XREReceiver) !== "undefined") {
            XREReceiver.onEvent("onClosedCaptions", { setTrack: trackID });
        }
    }
}

//function to Change the Closed Captioning Style Options
function changeCCStyle() {
    var styleOption =  document.getElementById("ccStyles").selectedIndex; // get selected cc track
    if ((enableNativeCC) && (ccStatus === true)) {
        //if CC is enabled
        switch(styleOption) {
            case 0:
                    playerObj.setTextStyleOptions(JSON.stringify(ccOptions));
                    break;
            case 1:
                    playerObj.setTextStyleOptions(JSON.stringify(ccOptions1));
                    break;
            case 2:
                    playerObj.setTextStyleOptions(JSON.stringify(ccOptions2));
                    break;
        }
        console.log("Current closed caption style is :" + playerObj.getTextStyleOptions());
    } else if((!enableNativeCC) && (ccStatus === true) && (typeof(XREReceiver) !== "undefined")) {
        switch(styleOption) {
            case 0:
                    XREReceiver.onEvent("onClosedCaptions", { setOptions: xreCCOptions});
                    break;
            case 1:
                    XREReceiver.onEvent("onClosedCaptions", { setOptions: xreCCOptions1});
                    break;
            case 2:
                    XREReceiver.onEvent("onClosedCaptions", { setOptions: xreCCOptions2});
                    break;
        }
    }
}

//function to jump to user entered position
function jumpToPPosition() {
    if(document.getElementById("jumpPosition").value) {
        var position = Number(document.getElementById("jumpPosition").value)/1000;
        if (!isNaN(position)) {
            if(document.getElementById("seekCheck").checked) {
                // call old seek API
                playerObj.seek(position);
            } else {
                // call new seek API with support to seek with pause
                playerObj.pause();
                playerObj.seek(position, true);
            }
        }
        document.getElementById("jumpPosition").value = "";
    }
}

//function to toggle Metadata widget
function toggleTimedMetadata() {
    var metadataMod = document.getElementById('metadataModal');
    document.getElementById("metadataCheck").checked = !document.getElementById("metadataCheck").checked;
    if(document.getElementById("metadataCheck").checked) {
        metadataMod.style.display = "block";
    } else {
        metadataMod.style.display = "none";
    }
}

function loadNextAsset() {
    resetPlayer();
    resetUIOnNewAsset();
    urlIndex++;
    if (urlIndex >= urls.length) {
        urlIndex = 0;
    }
    loadUrl(urls[urlIndex], false);
}

function cacheNextAsset() {
    urlIndex++;
    if (urlIndex >= urls.length) {
        urlIndex = 0;
    }
    cacheStream(urls[urlIndex], false);
}

function loadPrevAsset() {
    resetPlayer();
    resetUIOnNewAsset();
    urlIndex--;
    if (urlIndex < 0) {
        urlIndex = urls.length - 1;
    }
    loadUrl(urls[urlIndex], false);
}

var HTML5PlayerControls = function() {
    this.init = function() {
        this.video = document.getElementById("video");

        // Buttons
        this.videoToggleButton = document.getElementById("videoToggleButton");
        this.ccButton = document.getElementById("ccButton");
        
        this.rwdButton = document.getElementById("rewindButton");
        this.skipBwdButton = document.getElementById("skipBackwardButton");
        this.playButton = document.getElementById("playOrPauseButton");
        this.skipFwdButton = document.getElementById("skipForwardButton");
        this.fwdButton = document.getElementById("fastForwardButton");
        
        this.muteButton = document.getElementById("muteVideoButton");
        this.resizeButton = document.getElementById("resizeButton");
        
        this.cacheOnlyButton = document.getElementById("cacheOnlyCheck");
        this.metadataLogButton = document.getElementById("metadataCheck");
        this.autoSeekButton = document.getElementById("seekCheck");
        this.jumpPositionInput = document.getElementById("jumpPosition");
        this.jumpButton = document.getElementById("jumpButton");
        
        this.videoFileList = document.getElementById("videoURLs");
        this.audioTracksList = document.getElementById("audioTracks");
        this.ccTracksList = document.getElementById("ccTracks");
        this.ccStylesList = document.getElementById("ccStyles");
        this.homeContentButton = document.getElementById('homeButton');

        // Seekbar
        this.seekBar = document.getElementById("seekBar");

        this.currentObj = this.playButton;
        this.componentsTop = [ this.videoFileList, this.audioTracksList, this.ccTracksList, this.ccStylesList, this.homeContentButton];
        this.componentsDebug = [ this.cacheOnlyButton, this.metadataLogButton, this.autoSeekButton, this.jumpPositionInput, this.jumpButton];
        this.componentsBottom = [this.videoToggleButton, this.ccButton, this.rwdButton, this.skipBwdButton, this.playButton, this.skipFwdButton, this.fwdButton, this.muteButton, this.resizeButton];

        this.currentPosTop = 0; // URL List button
        this.currentPosBottom = 4; // Play button
        this.currentPosDebug = 0; // Cache button
        this.dropDownListVisible = false;
        this.audioListVisible = false;
        this.ccListVisible = false;
        this.ccStyleListVisible = false;
        this.selectListIndex = 0;
        this.selectAudioListIndex = 0;
        this.selectCCListIndex = 0;
        this.selectCCStyleListIndex = 0;
        this.prevObj = null;
        this.addFocus();
        
        //this.seekBar.style.backgroundColor = "red";

        document.getElementById('ffModal').style.display = "none";
        document.getElementById('ffSpeed').style.display = "none";
    };

    this.reset = function() {

        var value = 0;
        this.playButton.src = "../icons/play.png";
        this.seekBar.value = value;
        this.seekBar.style.width = value+"%";
    };

    this.keyLeft = function() {
        this.gotoPrevious();
    };

    this.keyRight = function() {
        this.gotoNext();
    };

    this.keyUp = function() {
        // Hide main menu if visible
        if(mainMenuToggle) {
            //document.getElementById("bottomNavBar").style["-webkit-animation-name"] = "animatetobottom";
            //document.getElementById("bottomNavBar").style["animation-name"] = "animatetobottom";
            document.getElementById("bottomNavBar").style.display = "none";
            mainMenuToggle = !mainMenuToggle;
        }

        this.removeFocus();
        // Set current Object to first element in top config nav bar
        this.currentObj =  this.videoFileList;
        this.addFocus();

        if(!configMenuToggle) {
            // Show config menu on keyup
            document.getElementById("topNavBar").style.display = "block";
            // Slide down animation
            document.getElementById("topNavBar").style["-webkit-animation-name"] = "animatefromtop";
            //document.getElementById("topNavBar").style["animation-name"] = "animatefromtop";
            configMenuToggle = !configMenuToggle;
        } else if(configMenuToggle && (!( this.dropDownListVisible|| this.audioListVisible || this.ccListVisible || this.ccStyleListVisible))) {
            // Hide config menu if already visible and no other element is active
            // Slide up animation
            document.getElementById("topNavBar").style["-webkit-animation-name"] = "animatetotop";
            //document.getElementById("topNavBar").style["animation-name"] = "animatetotop";
            
            setTimeout(function() {
                document.getElementById("topNavBar").style.display = "none";
            }, 400);

            configMenuToggle = !configMenuToggle;
        } else {
            if ((this.componentsTop[this.currentPosTop] == this.audioTracksList) && (this.audioListVisible)) {
                this.prevAudioSelect();
            } else if ((this.componentsTop[this.currentPosTop] == this.videoFileList) && (this.dropDownListVisible)) {
                this.prevVideoSelect();
            } else if ((this.componentsTop[this.currentPosTop] == this.ccTracksList) && (this.ccListVisible)) {
                this.prevCCSelect();
            } else if ((this.componentsTop[this.currentPosTop] == this.ccStylesList) && (this.ccStyleListVisible)) {
                this.prevCCStyleSelect();
            } else if ((this.componentsTop[this.currentPosTop] == this.playButton) || (this.componentsTop[this.currentPosTop] == this.videoToggleButton) || (this.componentsTop[this.currentPosTop] == this.rwdButton) || (this.componentsTop[this.currentPosTop] == this.skipBwdButton) || (this.componentsTop[this.currentPosTop] == this.skipFwdButton) || (this.componentsTop[this.currentPosTop] == this.fwdButton) || (this.componentsTop[this.currentPosTop] == this.muteButton) || (this.componentsTop[this.currentPosTop] == this.ccButton)) {
                //when a keyUp is received from the buttons in the bottom navigation bar
                this.removeFocus();
                this.currentObj = this.videoFileList;
                //move focus to the first element in the top navigation bar
                this.currentPosTop = this.componentsTop.indexOf(this.videoFileList);
                this.addFocus();
            }

        }

    };

    this.keyDown = function() {
        if(!configMenuToggle) {
            // If config menu is hidden
            if(!mainMenuToggle) {
                // Show main menu buttons on keydown
                document.getElementById("bottomNavBar").style.display = "block";
                // Set current object to play button
                this.removeFocus();
                this.currentObj = this.playButton;
                //move focus to the first element in the bottom navigation bar
                this.currentPosBottom = 4;
                this.addFocus();
            } else if(mainMenuToggle) {
                // Hide main menu if already visible
                // Slide down animation
                document.getElementById("bottomNavBar").style.display = "none";
            }
            mainMenuToggle = !mainMenuToggle;
        } else {
            // If any of the drop down list is active do traverse in them
            if(this.dropDownListVisible || this.audioListVisible || this.ccListVisible || this.ccStyleListVisible) {
                // If Config Menu is displayed
                if ((this.componentsTop[this.currentPosTop] == this.audioTracksList) && (this.audioListVisible)) {
                    this.nextAudioSelect();
                } else if ((this.componentsTop[this.currentPosTop] == this.videoFileList) && (this.dropDownListVisible)) {
                    this.nextVideoSelect();
                } else if ((this.componentsTop[this.currentPosTop] == this.ccTracksList) && (this.ccListVisible)) {
                    this.nextCCSelect();
                } else if ((this.componentsTop[this.currentPosTop] == this.ccStylesList) && (this.ccStyleListVisible)) {
                    this.nextCCStyleSelect();
                } else if ((this.componentsTop[this.currentPosTop] == this.audioTracksList) || (this.componentsTop[this.currentPosTop] == this.ccTracksList) || (this.componentsTop[this.currentPosTop] == this.ccStylesList) || (this.componentsTop[this.currentPosTop] == this.videoFileList) || (this.componentsTop[this.currentPosTop] == this.cacheOnlyButton) || (this.componentsTop[this.currentPosTop] == this.autoSeekButton) || (this.componentsTop[this.currentPosTop] == this.jumpPositionInput) || (this.componentsTop[this.currentPosTop] == this.jumpButton) || (this.componentsTop[this.currentPosTop] == this.metadataLogButton) || (this.componentsTop[this.currentPosTop] == this.homeContentButton)) {
                    //when a keyDown is received from the buttons in the top navigation bar
                    this.removeFocus();
                    this.currentObj = this.playButton;
                    //move focus to the first element in the bottom navigation bar
                    this.currentPosTop = 0;
                    this.addFocus();
                }
            } else {
                // Else close Config Menu and Show Main Menu
                document.getElementById("topNavBar").style["-webkit-animation-name"] = "animatetotop";
                //document.getElementById("topNavBar").style["animation-name"] = "animatetotop";
                setTimeout(function() {
                    document.getElementById("topNavBar").style.display = "none";
                }, 400);
                configMenuToggle = !configMenuToggle;

                document.getElementById("bottomNavBar").style.display = "block";
                // Set current object to play button
                this.removeFocus();
                this.currentObj = this.playButton;
                //move focus to the first element in the bottom navigation bar
                this.currentPosBottom = 4;
                this.addFocus();
                mainMenuToggle = !mainMenuToggle;
            }
        }
    };

    this.prevVideoSelect = function() {
        if (this.selectListIndex > 0) {
            this.selectListIndex--;
        } else {
            this.selectListIndex = this.videoFileList.options.length - 1;
        }
        this.videoFileList.options[this.selectListIndex].selected = true;
    };

    this.nextVideoSelect = function() {
        if (this.selectListIndex < this.videoFileList.options.length - 1) {
            this.selectListIndex++;
        } else {
            this.selectListIndex = 0;
        }
        this.videoFileList.options[this.selectListIndex].selected = true;
    };

    this.prevAudioSelect = function() {
        if (this.selectAudioListIndex > 0) {
            this.selectAudioListIndex--;
        } else {
            this.selectAudioListIndex = this.audioTracksList.options.length - 1;
        }
        this.audioTracksList.options[this.selectAudioListIndex].selected = true;
    };

    this.nextAudioSelect = function() {
        if (this.selectAudioListIndex < this.audioTracksList.options.length - 1) {
            this.selectAudioListIndex++;
        } else {
            this.selectAudioListIndex = 0;
        }
        this.audioTracksList.options[this.selectAudioListIndex].selected = true;
    };

    this.prevCCSelect = function() {
        if (this.selectCCListIndex > 0) {
            this.selectCCListIndex--;
        } else {
            this.selectCCListIndex = this.ccTracksList.options.length - 1;
        }
        this.ccTracksList.options[this.selectCCListIndex].selected = true;
    };

    this.nextCCSelect = function() {
        if (this.selectCCListIndex < this.ccTracksList.options.length - 1) {
            this.selectCCListIndex++;
        } else {
            this.selectCCListIndex = 0;
        }
        this.ccTracksList.options[this.selectCCListIndex].selected = true;
    };

    this.prevCCStyleSelect = function() {
        if (this.selectCCStyleListIndex > 0) {
            this.selectCCStyleListIndex--;
        } else {
            this.selectCCStyleListIndex = this.ccStylesList.options.length - 1;
        }
        this.ccStylesList.options[this.selectCCStyleListIndex].selected = true;
    };

    this.nextCCStyleSelect = function() {
        if (this.selectCCStyleListIndex < this.ccStylesList.options.length - 1) {
            this.selectCCStyleListIndex++;
        } else {
            this.selectCCStyleListIndex = 0;
        }
        this.ccStylesList.options[this.selectCCStyleListIndex].selected = true;
    };

    this.showDropDown = function() {
        this.dropDownListVisible = true;
        var n = this.videoFileList.options.length;
        this.videoFileList.size = n;
    };

    this.hideDropDown = function() {
        this.dropDownListVisible = false;
        this.videoFileList.size = 1;
    };

    this.showAudioDropDown = function() {
        this.audioListVisible = true;
        var n = this.audioTracksList.options.length;
        this.audioTracksList.size = n;
    };

    this.hideAudioDropDown = function() {
        this.audioListVisible = false;
        this.audioTracksList.size = 1;
    };

    
    this.showCCDropDown = function() {
        this.ccListVisible = true;
        var n = this.ccTracksList.options.length;
        this.ccTracksList.size = n;
    };

    this.hideCCDropDown = function() {
        this.ccListVisible = false;
        this.ccTracksList.size = 1;
    };

    this.showCCStyleDropDown = function() {
        this.ccStyleListVisible = true;
        var n = this.ccStylesList.options.length;
        this.ccStylesList.size = n;
    };

    this.hideCCStyleDropDown = function() {
        this.ccStyleListVisible = false;
        this.ccStylesList.size = 1;
    };

    this.ok = function() {
        if(mainMenuToggle) {
            // If main menu nav bar is on display
            switch (this.currentPosBottom) {
                case 0:
                        toggleVideo();
                        break;
                case 1:
                        toggleCC();
                        break;
                case 2:
                        fastrwd();
                        break;
                case 3:
                        skipBackward();
                        break;
                case 4:
                        playPause();
                        break;
                case 5:
                        skipForward();
                        break;
                case 6:
                        fastfwd();
                        break;
                case 7:
                        mutePlayer();
                        break;
                case 8:
                        resizeVideo();
                        break;
            };

        } else if(configMenuToggle) {
            // If config menu nav bar is on display
            switch (this.currentPosTop) {
                case 0:
                    if (this.dropDownListVisible == false) {
                        this.showDropDown();
                    } else {
                        this.hideDropDown();
                        getVideo(document.getElementById("cacheOnlyCheck").checked);
                    }
                    break;
                case 1:
                        if (this.audioListVisible == false) {
                            this.showAudioDropDown();
                        } else {
                            this.hideAudioDropDown();
                            changeAudioTrack();
                        }
                        break;
                case 2:
                        if (this.ccListVisible == false) {
                            this.showCCDropDown();
                        } else {
                            this.hideCCDropDown();
                            changeCCTrack();
                        }
                        break;
                case 3:
                        if (this.ccStyleListVisible == false) {
                            this.showCCStyleDropDown();
                        } else {
                            this.hideCCStyleDropDown();
                            changeCCStyle();
                        }
                        break;
                case 4:
                        goToHome();
                        break;
            };
        }  else if(debugMenuToggle) {
            // If debug menu nav bar is on display
            switch (this.currentPosDebug) {
                case 0: 
                        document.getElementById("cacheOnlyCheck").checked = !document.getElementById("cacheOnlyCheck").checked;
                        break;
                case 1: 
                        toggleTimedMetadata();
                        break;  
                case 2: 
                        document.getElementById("seekCheck").checked = !document.getElementById("seekCheck").checked;
                        break;
                case 4: 
                        jumpToPPosition();
                        break;    
                    
            }
        }
    };

    this.gotoNext = function() {
        this.removeFocus();
        if(mainMenuToggle) {
            // If main menu nav bar is on display
            if (this.currentPosBottom < this.componentsBottom.length - 1) {
                this.currentPosBottom++;
            } else {
                this.currentPosBottom = 0;
            }
            this.currentObj = this.componentsBottom[this.currentPosBottom];
        } else if(configMenuToggle) {
            // If config menu nav bar is on display
            if (this.currentPosTop < this.componentsTop.length - 1) {
                this.currentPosTop++;
            } else {
                this.currentPosTop = 0;
            }
            this.currentObj = this.componentsTop[this.currentPosTop];
        } else if(debugMenuToggle) {
            // If debug menu nav bar is on display
            if (this.currentPosDebug < this.componentsDebug.length - 1) {
                this.currentPosDebug++;
            } else {
                this.currentPosDebug = 0;
            }
            this.currentObj = this.componentsDebug[this.currentPosDebug];
            addStyleToDebugButtons(this.currentPosDebug);
        }
        this.addFocus();
    };

    this.gotoPrevious = function() {
        this.removeFocus();
        if(mainMenuToggle) {
            // If main menu nav bar is on display
            if (this.currentPosBottom > 0) {
                this.currentPosBottom--;
            } else {
                this.currentPosBottom = this.componentsBottom.length - 1;
            }
            this.currentObj = this.componentsBottom[this.currentPosBottom];
        } else if(configMenuToggle) {
            // If config menu nav bar is on display
            if (this.currentPosTop > 0) {
                this.currentPosTop--;
            } else {
                this.currentPosTop = this.componentsTop.length - 1;
            }
            this.currentObj = this.componentsTop[this.currentPosTop];
        } else if(debugMenuToggle) {
            // If debug menu nav bar is on display
            if (this.currentPosDebug > 0) {
                this.currentPosDebug--;
            } else {
                this.currentPosDebug = this.componentsDebug.length - 1;
            }
            this.currentObj = this.componentsDebug[this.currentPosDebug];
            addStyleToDebugButtons(this.currentPosDebug);
        }    
        this.addFocus();
    };

    function addStyleToDebugButtons(currentPosDebug) {
        const root = document.querySelector(":root"); //grabbing the root element
        switch(currentPosDebug) {
            case 0: 
                    root.style.setProperty("--pseudo-cache-backgroundcolor", '#6d6d6d');
                    root.style.setProperty("--pseudo-seek-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-metadata-backgroundcolor", '#313232');
                    break;
            case 1:
                    root.style.setProperty("--pseudo-cache-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-seek-backgroundcolor", '#6d6d6d');
                    root.style.setProperty("--pseudo-metadata-backgroundcolor", '#313232');
                    break;
            case 2: 
                    root.style.setProperty("--pseudo-cache-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-seek-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-metadata-backgroundcolor", '#6d6d6d');
                    break;
            case 3:
            case 4:
                    root.style.setProperty("--pseudo-cache-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-seek-backgroundcolor", '#313232');
                    root.style.setProperty("--pseudo-metadata-backgroundcolor", '#313232');
                    break;
        }
    }

    this.addFocus = function() {
        if (this.currentObj) {
            this.currentObj.classList.add("focus");
        } else {
            this.currentObj.focus();
        }
    };

    this.removeFocus = function() {
        if (this.currentObj) {
            this.currentObj.classList.remove("focus");
        } else {
            this.currentObj.blur();
        }
    };

    this.keyEventHandler = function(e, type) {
        var keyCode = e.which || e.keyCode;
        console.log("UVE Pressed keycode" + keyCode);
        e.preventDefault();
        if (type == "keydown") {
            switch (keyCode) {
                case 37: // Left Arrow
                        this.keyLeft();
                        break;
                case 38: // Up Arrow
                        this.keyUp();
                        break;
                case 39: // Right Arrow
                        this.keyRight();
                        break;
                case 40: // Down Arrow
                        this.keyDown();
                        break;
                case 13: // Enter
                        if(disableButtons) {
                            // If playback error modal is ON, hide it on clicking 'OK'
                            this.dismissModalDialog();
                        } else {
                            this.ok();
                        }
                        break;
                case 88: // X
		        case 34:
                        skipBackward();
                        break;
                case 90: // Z
		        case 33:
                        skipForward();
                        break;
                case 32:
                        if(disableButtons) {
                            // If playback error modal is ON, hide it on clicking 'OK'
                            this.dismissModalDialog();
                        } else {
                            this.ok();
                        }
                        break;
		        case 179:
                case 80: // P
                        playPause();
                        break;
                case 113: // F2
                        mutePlayer();
                        break;
                case 82: // R
		        case 227:
                        fastrwd();
                        break;
                case 70: // F
		        case 228:
                        fastfwd();
                        break;
                case 117: // F6
                        overlayController();
                        break;
                case 85: // U
                        loadNextAsset();
                        break;
                case 68: // D
                        loadPrevAsset();
                        break;
                case 48: // Number 0
                case 49: // Number 1
                case 50: // Number 2
                case 51: // Number 3
                case 52: // Number 4
                case 53: // Number 5
                case 54: // Number 6
                case 55: // Number 7
                case 56: // Number 8
                case 57: // Number 9
                         // If keypress is for input to the progress position field
                         if((this.currentObj === this.jumpPositionInput) && !disableButtons) {
                             document.getElementById("jumpPosition").value =  document.getElementById("jumpPosition").value + String(e.key);
                         }
                         break;
                default:
                        break;
            }
        }
        // Get current Object ID
        currentObjID = this.currentObj.id;
        return false;
    }

    this.dismissModalDialog = function() {
        //If clicked OK on overlay modal hide it
        document.getElementById('errorModal').style.display = "none";
        this.currentObj = this.videoFileList;
        this.currentPosTop = 0;
        // Move focus to the video url list
        this.currentPosTop = this.componentsTop.indexOf(this.videoFileList);
        this.addFocus();
        disableButtons = false;
    }
};

// Function to change the opacity of the buttons
function changeButtonOpacity(opacity) {
    document.getElementById('jumpPosition').style.opacity = opacity;
    document.getElementById('jumpButton').style.opacity = opacity;
    document.getElementById('playOrPauseButton').style.opacity = opacity;
    document.getElementById('videoToggleButton').style.opacity = opacity;
    document.getElementById('rewindButton').style.opacity = opacity;
    document.getElementById('skipBackwardButton').style.opacity = opacity;
    document.getElementById('skipForwardButton').style.opacity = opacity;
    document.getElementById('fastForwardButton').style.opacity = opacity;
    document.getElementById('muteVideoButton').style.opacity = opacity;
    document.getElementById('ccButton').style.opacity = opacity;
    document.getElementById('ccTracks').style.opacity = opacity;
    document.getElementById('ccStyles').style.opacity = opacity;
    document.getElementById('homeButton').style.opacity = opacity;
}

function overlayController() {
    var navBar = document.getElementById('bottomNavBar');
    var navBarNext = document.getElementById('topNavBar');
    // Get the modal
    if(navBar.style.display == "block") {
        navBar.style.display = "none";
    } else {
        navBar.style.display = "block";
    }
    if(navBarNext.style.display == "block") {
        navBarNext.style.display = "none";
    } else {
        navBarNext.style.display = "block";
    }
};

function createBitrateList(availableBitrates) {
    bitrateList = [];
    for (var iter = 0; iter < availableBitrates.length; iter++) {
        bitrate = (availableBitrates[iter] / 1000000).toFixed(1);
        bitrateList.push(bitrate);
    }
    document.getElementById("availableBitratesList").innerHTML = bitrateList;
};

function showTrickmodeOverlay(speed) {
    document.getElementById('ffSpeed').innerHTML = Math.abs(speed)+ "x";
    if (speed > 0) {
        document.getElementById('ffModal').style["-webkit-transform"]= "scaleX(1)";
    } else {
        document.getElementById('ffModal').style["-webkit-transform"]= "scaleX(-1)";
    }

    //Display Fast Forward modal
    document.getElementById('ffModal').style.display = "block";
    document.getElementById('ffSpeed').style.display = "block";

    //Set timeout to hide
    setTimeout(function() {
        document.getElementById('ffModal').style.display = "none";
        document.getElementById('ffSpeed').style.display = "none";
    }, 2000);
};

// Convert seconds to hours
function convertSStoHr(videoTime) {
    var hhTime = Math.floor(videoTime / 3600);
    var mmTime = Math.floor((videoTime - (hhTime * 3600)) / 60);
    var ssTime = videoTime - (hhTime * 3600) - (mmTime * 60);
    ssTime = Math.round(ssTime);

    var timeFormat = (hhTime < 10 ? "0" + hhTime : hhTime);
        timeFormat += ":" + (mmTime < 10 ? "0" + mmTime : mmTime);
        timeFormat += ":" + (ssTime  < 10 ? "0" + ssTime : ssTime);

    return timeFormat;
};


function resetUIOnNewAsset(){
    controlObj.reset();
    document.getElementById("muteIcon").src = "../icons/unMute.png";
    document.getElementById("currentDuration").innerHTML = "00:00:00";
    document.getElementById("positionInSeconds").innerHTML = "0s";
    document.getElementById("totalDuration").innerHTML = "00:00:00";
    document.getElementById('ffSpeed').innerHTML = "";
    document.getElementById('ffModal').style.display = "none";
    document.getElementById('ffSpeed').style.display = "none";
    document.getElementById("jumpPosition").value = "";
    document.getElementById("metadataContent").innerHTML = "";
};

function initPlayerControls() {

    controlObj = new HTML5PlayerControls();
    controlObj.init();
    if (document.addEventListener) {
        document.addEventListener("keydown", function(e) {
            return controlObj.keyEventHandler(e, "keydown");
        });
    }

    //to load URL select field
	if(urls) {
        // Iteratively adding all the options to videoURLs
        for (var iter = 0; iter < urls.length; iter++) {
            var option = document.createElement("option");
            option.value = urls[iter].url;
            option.text = urls[iter].name;
            document.getElementById("videoURLs").add(option);
        }
    }
};


// Functions

// basic toggle (open/close) function
// "classList.toggle(className)" toggles 'opened' class
const toggleDropdown = (event) => {
    const dropdown = document.querySelector('.dropdown');
    event.stopPropagation();
    dropdown.classList.toggle('opened');
  };
  // option selection from dropdown list
  // used "event.currentTarget" to specify the selected option
  // after option is chosen, its "textContent" value being copied to input's value
  const selectOption = (event) => {
    const input = document.querySelector('.input');
    input.value = event.currentTarget.textContent;
  };
  
  // we want the dropdown list to close when clicked outside of it
  // ex: no option was selected
  // we do a simple check below
  // if dropdown list is in opened state
  // then remove the ".opened" class
  const closeDropdownFromOutside = () => {
    const dropdown = document.querySelector('.dropdown');
    if (dropdown.classList.contains('opened')) {
      dropdown.classList.remove('opened');
    }
  };

