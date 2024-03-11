var initConfig = {
    // Use single GStreamer Pipeline
    useSinglePipeline: false,

    // enable info logging
    info: true,
};

async function TST_UVE_Pipeline_VerifyBlankPeriod(vid_capture, singlePipeline, periodNumber){
    if (vid_capture.isActive()){
        var assertMessage = "";

        console.log("Verify Blank Period: #" + periodNumber + ", SinglePipeline: ", singlePipeline);

        // Verify the Blank Period
        var blankingData = await vid_capture.GetBlankingStatus();

        // Get the Capture status and display the Blank Period
        var blankPeriods   = blankingData[0];
        var blankLastTime  = blankingData[1];
        var blankLastStart = blankingData[2];
        var blankLastEnd   = blankingData[3];
        var blankLastCount = blankingData[4];
        blank_str = "Black Periods: " + blankPeriods + " Last duration: " + blankLastTime + "s";
        blank_str += "<br> " + blankLastCount + " Frames : Start (" + blankLastStart + "), End (" + blankLastEnd + ")";
        TST_INFO(blank_str);

        if (singlePipeline){
            // Single Pipeline Mode - ON, There should be no blank periods in this test
            if (blankPeriods != 0){
                assertMessage = ("No blank periods expected in Single Pipeline Mode: " + blankPeriods)
            }
        }
        else {
            // Multi Pipeline Mode - Verify the number of blank periods matches the expected, and they are very roughly the expected length
            if (blankPeriods != periodNumber){
                assertMessage = ("Number of Blank Periods Mismatch: " + blankPeriods + ", " + periodNumber)
            }

            if ((blankLastCount <= 5) || (blankLastCount >= 50)){
                assertMessage = ("Blank Period out of range: " + blankLastCount)
            }
        }

        // Ensure the Video Capture is correctly Stopped before issuing the Assert to stop the test
        if (assertMessage != ""){
            await vid_capture.Stop();
            await vid_capture.Term();
            TST_ASSERT_FAIL_FATAL(assertMessage);
        }
    }
    else {
        TST_INFO("Video Capture not in use for this test run");
    }
};

async function TST_UVE_PipelineTests(testName, singlePipeline) {
    TST_START(testName);

    TST_INFO("Init Vid Capture... ");
    var vid_capture = new VideoCapture();
    if (vid_capture.isActive()){
        await vid_capture.Init();
        var isInit = await vid_capture.WaitForInit();
        TST_ASSERT ((isInit == true), ("Unexpected Init Status - Got: " + isInit + ", Expected: true"));
    }
    else {
        TST_INFO("Video Capture not in use for this test run");
    }

    // Create Player instances for the Main content and Ads
    var aamp_main_player = new AAMPPlayer("MAIN-PLAYER-" + (singlePipeline ? "ON" : "OFF"));
    var aamp_ad_1_player = new AAMPPlayer("AD-1-PLAYER-" + (singlePipeline ? "ON" : "OFF"));
    var aamp_ad_2_player = new AAMPPlayer("AD-2-PLAYER-" + (singlePipeline ? "ON" : "OFF"));
    var aamp_ad_3_player = new AAMPPlayer("AD-3-PLAYER-" + (singlePipeline ? "ON" : "OFF"));

    // Initialise config
    TST_INFO("Init config flags");
    initConfig.useSinglePipeline = singlePipeline;
    aamp_main_player.player.initConfig(initConfig);
    aamp_ad_1_player.player.initConfig(initConfig);
    aamp_ad_2_player.player.initConfig(initConfig);
    aamp_ad_3_player.player.initConfig(initConfig);

    // URLs for the Main Content and Ads
    AD_HOST = "https://cpetestutility.stb.r53.xcal.tv"
    var main_url = AD_HOST + "/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd";  // 60sec - ad2 (lifeboat)
    var ad_1_url = AD_HOST + "/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd";  // 40sec - ad1 (telecoms)
    var ad_2_url = AD_HOST + "/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd"; // 30sec - ad3 (bet)
    var ad_3_url = AD_HOST + "/aamptest/ads/ad6/hsar1103-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad20/ce5b8762-d14a-4f92-ba34-13d74e34d6ac/1628252375289/AD/HD/manifest.mpd"; // 25sec - ad6 (one)

    TST_INFO("Playing Main Content");
    await aamp_main_player.Load(main_url);
    await aamp_main_player.VerifyPlayback(5, 1);

    if (vid_capture.isActive()){
        // Start the capture device for a maximum of 5 mins (300s)
        TST_INFO("Start Capture: 300(s)");

        var filename = "VID_000" + "_SinglePipeline_" + (singlePipeline ? "ON" : "OFF") + ".mp4";
        await vid_capture.Capture(filename, 300);
        var isCapturing = await vid_capture.WaitForCapturing();
        TST_ASSERT ((isCapturing == true), ("Unexpected Capturing Status - Got: " + isCapturing + ", Expected: true"));
    }

    // Wait 5 further seconds
    await aamp_main_player.VerifyPlayback(5, 1);

    // Pre-Load Ad 1, but Don't Play
    TST_INFO("Pre-Load Ad 1");
    await aamp_ad_1_player.Load(ad_1_url, false);
    await aamp_main_player.VerifyPlayback(5, 1);

    // Detach main content and start Ad 1 playback
    TST_INFO("Playing Ad 1");
    aamp_main_player.player.detach();
    await aamp_ad_1_player.Play();
    await aamp_ad_1_player.VerifyPlayback(5, 1);

    // Verify the Blank Period
    await TST_UVE_Pipeline_VerifyBlankPeriod(vid_capture, singlePipeline, 1);

    // Wait 5 further seconds
    await aamp_ad_1_player.VerifyPlayback(5, 1);


    // Pre-Load Ad 2, but Don't Play
    TST_INFO("Pre-Load Ad 2");
    await aamp_ad_2_player.Load(ad_2_url, false);
    await aamp_ad_1_player.VerifyPlayback(5, 1);

    // Detach main content and start Ad 1 playback
    TST_INFO("Playing Ad 2");
    aamp_ad_1_player.player.detach();
    await aamp_ad_2_player.Play();
    await aamp_ad_2_player.VerifyPlayback(5, 1);
    await aamp_ad_1_player.Stop(false);

    // Verify the Blank Period
    await TST_UVE_Pipeline_VerifyBlankPeriod(vid_capture, singlePipeline, 2);

    // Wait 5 further seconds
    await aamp_ad_2_player.VerifyPlayback(5, 1);

    // Pre-Load Ad 3, but Don't Play
    TST_INFO("Pre-Load Ad 3");
    await aamp_ad_3_player.Load(ad_3_url, false);
    await aamp_ad_2_player.VerifyPlayback(5, 1);

    // Detach main content and start Ad 1 playback
    TST_INFO("Playing Ad 3");
    aamp_ad_2_player.player.detach();
    await aamp_ad_3_player.Play();
    await aamp_ad_3_player.VerifyPlayback(5, 1);
    await aamp_ad_2_player.Stop(false);

    // Verify the Blank Period
    await TST_UVE_Pipeline_VerifyBlankPeriod(vid_capture, singlePipeline, 3);

    // Wait 5 further seconds
    await aamp_ad_3_player.VerifyPlayback(5, 1);


    // Pre-Load & Seek, but Don't Play
    TST_INFO("Seek Main Content");
    await aamp_main_player.Seek(10, false);
    await aamp_ad_3_player.VerifyPlayback(5, 1);

    // Detach Ad 3 content and continue main
    TST_INFO("Playing Main Content");
    aamp_ad_3_player.player.detach();
    await aamp_main_player.Play();
    await aamp_main_player.VerifyPlayback(5, 1);
    await aamp_ad_3_player.Stop(false);

    // Verify the Blank Period
    await TST_UVE_Pipeline_VerifyBlankPeriod(vid_capture, singlePipeline, 4);

    // Wait 10 further seconds
    await aamp_main_player.VerifyPlayback(10, 1);

    // Stop the video capture
    await vid_capture.Stop();

    await aamp_main_player.Stop(false);
    aamp_main_player.destroy();
    aamp_ad_3_player.destroy();
    aamp_ad_2_player.destroy();
    aamp_ad_1_player.destroy();

    await vid_capture.Term();

    TST_END()
}
