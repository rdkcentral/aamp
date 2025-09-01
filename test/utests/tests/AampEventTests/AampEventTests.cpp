/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <gtest/gtest.h>
#include "AampEvent.h" 
#include "vttCue.h"

using namespace testing;

#define EFFECTIVE_URL "http://manifest.mpd"

class AAMPEventTests : public ::testing::Test {
protected:
    AAMPEvent *handler = nullptr;
    void SetUp() override {
        handler = new AAMPEvent();
    }

    void TearDown() override {
       delete handler;
        handler = nullptr;
    }

};

// Randomly generated UUIDv4
const std::string session_id {"12192978-da71-4da7-8335-76fbd9ae2ae9"};

const AAMPEventType allEventTypes[] = {
    AAMPEventType::AAMP_EVENT_ALL_EVENTS,
    AAMPEventType::AAMP_EVENT_TUNED,
    AAMPEventType::AAMP_EVENT_TUNE_FAILED,
    AAMPEventType::AAMP_EVENT_SPEED_CHANGED,
    AAMPEventType::AAMP_MAX_NUM_EVENTS
};
// Generate test cases for all enum values using a loop
TEST_F(AAMPEventTests, GetTypeTest) {

    for (AAMPEventType eventType : allEventTypes) {
        AAMPEventObject event(eventType, session_id);
        EXPECT_EQ(event.getType(), eventType);
        EXPECT_EQ(event.GetSessionId(), session_id);
    }
}
// getType() method of AAMPEventObject
TEST_F(AAMPEventTests, GetTypeTest1) {
    AAMPEventType eventType = AAMPEventType::AAMP_MAX_NUM_EVENTS;
    AAMPEventObject event(eventType, session_id);
    EXPECT_EQ(event.getType(), eventType);
    EXPECT_EQ(event.GetSessionId(), session_id);

    AAMPEventObject event1(AAMPEventType::AAMP_EVENT_TUNED, session_id);
    EXPECT_EQ(event1.getType(), AAMPEventType::AAMP_EVENT_TUNED);


    AAMPEventObject event2(AAMPEventType::AAMP_EVENT_BLOCKED, session_id);
    EXPECT_EQ(event2.getType(), AAMPEventType::AAMP_EVENT_BLOCKED);

    
    AAMPEventObject event3(AAMPEventType::AAMP_EVENT_PLAYLIST_INDEXED, session_id);
    EXPECT_EQ(event3.getType(), AAMPEventType::AAMP_EVENT_PLAYLIST_INDEXED);
}

// Test fixture for MediaErrorEvent
class MediaErrorEventTest : public testing::Test{
protected:
    
    void SetUp() override {
        errorEvent = new MediaErrorEvent(AAMP_TUNE_FAILURE_UNKNOWN,1,"Test",false,0,0,0,"", session_id);
    }
    void TearDown() override {
        delete errorEvent;
    }
    MediaErrorEvent* errorEvent;
};

TEST_F(MediaErrorEventTest, MediaErrorEventMethodsTest) {
    MediaErrorEvent errorEvent(
        AAMPTuneFailure::AAMP_TUNE_INIT_FAILED,
        0, 
        "Tune failure due to initialization error",
        true, 
        456, 
        789, 
        987, 
        "response data",
        std::string{}
    );

    EXPECT_EQ(errorEvent.getFailure(), AAMPTuneFailure::AAMP_TUNE_INIT_FAILED);
    EXPECT_EQ(errorEvent.getCode(), 0);
    EXPECT_EQ(errorEvent.getDescription(), "Tune failure due to initialization error");
    EXPECT_EQ(errorEvent.getResponseData(), "response data");
    EXPECT_TRUE(errorEvent.shouldRetry());
    EXPECT_EQ(errorEvent.getClass(), 456);
    EXPECT_EQ(errorEvent.getReason(), 789);
    EXPECT_EQ(errorEvent.getBusinessStatus(), 987);
    EXPECT_EQ(errorEvent.GetSessionId(), "");
}
 
TEST_F(MediaErrorEventTest, MediaErrorEventMethodsanotherTest) {

    EXPECT_EQ(errorEvent->getFailure(), AAMPTuneFailure::AAMP_TUNE_FAILURE_UNKNOWN);
    EXPECT_EQ(errorEvent->getCode(), 1);
    EXPECT_EQ(errorEvent->getDescription(), "Test");
    EXPECT_EQ(errorEvent->getResponseData(), "");
    EXPECT_FALSE(errorEvent->shouldRetry());
    EXPECT_EQ(errorEvent->getClass(), 0);
    EXPECT_EQ(errorEvent->getReason(), 0);
    EXPECT_EQ(errorEvent->getBusinessStatus(), 0);
    EXPECT_EQ(errorEvent->GetSessionId(), session_id);
}


// Test fixture for SpeedChangedEvent
class SpeedChangedEventTest : public testing::Test {
protected:
    void SetUp() override {
        speedEvent = new SpeedChangedEvent(2.0, session_id);  
    }

    void TearDown() override {
        delete speedEvent;
    }

    SpeedChangedEvent* speedEvent;
};

// Test for getRate() function of SpeedChangedEvent
TEST_F(SpeedChangedEventTest, GetRateTest) {
    EXPECT_FLOAT_EQ(speedEvent->getRate(), 2.0);
}

// Test fixture for ProgressEvent
class ProgressEventTest : public testing::Test {
protected:
    void SetUp() override {
        progressEvent = new ProgressEvent(
            1000.0,   // duration
            500.0,    // position
            0.0,      // start
            2000.0,   // end
            1.0,      // speed
            1234567,  // pts
            800.0,    // video buffered duration
            800.0,    // audio buffered duration
            "00:00:00:00",  // sei timecode
            5.0,      // live latency
            500,   // profile bandwidth
            1000,  // network bandwidth
            2,      //currentPlayRate
            session_id // Session ID
        );
    }

    void TearDown() override {
        delete progressEvent;
    }

    ProgressEvent* progressEvent;
};

// Test various getter functions of ProgressEvent
TEST_F(ProgressEventTest, GetFunctionsTest) {
    EXPECT_DOUBLE_EQ(progressEvent->getDuration(), 1000.0);
    EXPECT_DOUBLE_EQ(progressEvent->getPosition(), 500.0);
    EXPECT_DOUBLE_EQ(progressEvent->getStart(), 0.0);
    EXPECT_DOUBLE_EQ(progressEvent->getEnd(), 2000.0);
    EXPECT_FLOAT_EQ(progressEvent->getSpeed(), 1.0);
    EXPECT_EQ(progressEvent->getPTS(), 1234567);
    EXPECT_DOUBLE_EQ(progressEvent->getVideoBufferedDuration(), 800.0);
    EXPECT_DOUBLE_EQ(progressEvent->getAudioBufferedDuration(), 800.0);
    EXPECT_STREQ(progressEvent->getSEITimeCode(), "00:00:00:00");
    EXPECT_DOUBLE_EQ(progressEvent->getLiveLatency(), 5.0);
    EXPECT_EQ(progressEvent->getProfileBandwidth(), 500);
    EXPECT_EQ(progressEvent->getNetworkBandwidth(), 1000);
    EXPECT_EQ(progressEvent->getCurrentPlayRate(), 2);

}
// Test fixture for CCHandleEvent
class CCHandleEventTest : public testing::Test {
protected:
    void SetUp() override {
        ccHandleEvent = new CCHandleEvent(12345, session_id);
    }

    void TearDown() override {
        delete ccHandleEvent;
    }

    CCHandleEvent* ccHandleEvent;
};

// Test the getCCHandle function of CCHandleEvent
TEST_F(CCHandleEventTest, GetCCHandleTest) {
    EXPECT_EQ(ccHandleEvent->getCCHandle(), 12345);
}
// Test fixture for MediaMetadataEvent
class MediaMetadataEventTest : public testing::Test {
protected:
    void SetUp() override {
        event = new MediaMetadataEvent(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);
    }

    void TearDown() override {
        delete event;
    }

    MediaMetadataEvent* event;
};

// Test case for constructor and getter methods
TEST_F(MediaMetadataEventTest, ConstructorAndGetterTest) {
    long duration = 1000;
    int width = 1920;
    int height = 1080;
    bool hasDrm = true;
    bool isLive = true;
    std::string drmType = "DRM Type";
    double programStartTime = 123456.789;
    int tsbDepthMs = 123;

    MediaMetadataEvent event(duration, width, height, hasDrm, isLive, drmType, programStartTime, tsbDepthMs, session_id, EFFECTIVE_URL);

    EXPECT_EQ(event.getDuration(), duration);
    EXPECT_EQ(event.getWidth(), width);
    EXPECT_EQ(event.getHeight(), height);
    EXPECT_EQ(event.hasDrm(), hasDrm);
    EXPECT_EQ(event.isLive(), isLive);
    EXPECT_EQ(event.getDrmType(), drmType);
    EXPECT_EQ(event.getProgramStartTime(), programStartTime);
    EXPECT_EQ(event.getUrl(), EFFECTIVE_URL);
}

TEST_F(MediaMetadataEventTest, EmptyLanguageListTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    const std::vector<std::string> &languages = event.getLanguages();
    EXPECT_EQ(languages.size(), 0);
}

TEST_F(MediaMetadataEventTest, AddSingleLanguageTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.addLanguage("Spanish");

    const std::vector<std::string> &languages = event.getLanguages();
    EXPECT_EQ(languages.size(), 1);
    EXPECT_EQ(languages[0], "Spanish");
}

TEST_F(MediaMetadataEventTest, AddMultipleLanguagesTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.addLanguage("French");
    event.addLanguage("German");
    event.addLanguage("Italian");
    event.addLanguage("English");
    event.addLanguage("Hindi");

    const std::vector<std::string> &languages = event.getLanguages();
    EXPECT_EQ(languages.size(), 5);
    EXPECT_EQ(languages[0], "French");
    EXPECT_EQ(languages[1], "German");
    EXPECT_EQ(languages[2], "Italian");
    EXPECT_EQ(languages[3], "English");
    EXPECT_EQ(languages[4], "Hindi");
    EXPECT_EQ(event.getLanguagesCount(), 5);
}
// Test case for addBitrate, getBitrates, and getBitratesCount methods
TEST_F(MediaMetadataEventTest, BitrateMethodsBoundaryTest) {
    // Create a MediaMetadataEvent instance
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    // Add bitrates with maximum and minimum values
    event.addBitrate(1); // Minimum bitrate
    event.addBitrate(std::numeric_limits<BitsPerSecond>::max()); // Maximum bitrate

    const std::vector<BitsPerSecond> &bitrates = event.getBitrates();
    ASSERT_EQ(bitrates.size(), 2);
    EXPECT_EQ(bitrates[0], 1);
    EXPECT_EQ(bitrates[1], std::numeric_limits<BitsPerSecond>::max());

    // Add audio bitrates with maximum and minimum values
    event.addAudioBitrate(1); // Minimum audio bitrate
    event.addAudioBitrate(std::numeric_limits<BitsPerSecond>::max()); // Maximum audio bitrate

    const std::vector<BitsPerSecond> &audioBitrates = event.getAudioBitrates();
    EXPECT_EQ(audioBitrates.size(), 2);
    EXPECT_EQ(audioBitrates[0], 1);
    EXPECT_EQ(audioBitrates[1], std::numeric_limits<BitsPerSecond>::max());
}

TEST_F(MediaMetadataEventTest, BitrateMethodsNegativeTest) {
    // Create a MediaMetadataEvent instance
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    // Add negative bitrates
    event.addBitrate(-100); // Negative bitrate

    const std::vector<BitsPerSecond> &bitrates = event.getBitrates();
    EXPECT_EQ(bitrates.size(), 1);
    EXPECT_EQ(bitrates[0], -100);

   // Add negative audio bitrates
    event.addAudioBitrate(-50); // Negative audio bitrate

    const std::vector<BitsPerSecond> &audioBitrates = event.getAudioBitrates();
    EXPECT_EQ(audioBitrates.size(), 1); 
    
}

TEST_F(MediaMetadataEventTest, BitrateMethodsTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.addBitrate(1000000); // 1 Mbps
    event.addBitrate(2000000); // 2 Mbps

    const std::vector<BitsPerSecond> &bitrates = event.getBitrates();
    EXPECT_EQ(bitrates.size(), 2);
    EXPECT_EQ(bitrates[0], 1000000);
    EXPECT_EQ(bitrates[1], 2000000);

    ASSERT_EQ(event.getBitratesCount(), 2);

    event.addAudioBitrate(128000); // 128 Kbps
    event.addAudioBitrate(256000); // 256 Kbps

    const std::vector<BitsPerSecond> &audioBitrates = event.getAudioBitrates();
    EXPECT_EQ(audioBitrates.size(), 2);
    EXPECT_EQ(audioBitrates[0], 128000);
    EXPECT_EQ(audioBitrates[1], 256000);
}
TEST_F(MediaMetadataEventTest, SupportedSpeedMethodsTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.addSupportedSpeed(1.0);
    event.addSupportedSpeed(1.5);

    const std::vector<float> &speeds = event.getSupportedSpeeds();
    EXPECT_EQ(speeds.size(), 2);
    EXPECT_FLOAT_EQ(speeds[0], 1.0);
    EXPECT_FLOAT_EQ(speeds[1], 1.5);

    EXPECT_EQ(event.getSupportedSpeedCount(), 2);
}

// Test case for SetVideoMetaData method and related getter methods
TEST_F(MediaMetadataEventTest, VideoMetaDataMethodsTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.SetVideoMetaData(30.0, VideoScanType::eVIDEOSCAN_PROGRESSIVE, 16, 9, "MPEG2", "DOLBY_VISION", "PG", 123);

    EXPECT_FLOAT_EQ(event.getFrameRate(), 30.0);
    EXPECT_EQ(event.getVideoScanType(), VideoScanType::eVIDEOSCAN_PROGRESSIVE);
    EXPECT_EQ(event.getAspectRatioWidth(), 16);
    EXPECT_EQ(event.getAspectRatioHeight(), 9);
    EXPECT_EQ(event.getVideoCodec(), "MPEG2");
    EXPECT_EQ(event.getHdrType(), "DOLBY_VISION");
    EXPECT_EQ(event.getRatings(), "PG");
    EXPECT_EQ(event.getSsi(), 123);
}
// Test case for SetAudioMetaData method and related getter methods
TEST_F(MediaMetadataEventTest, AudioMetaDataMethodsTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    event.SetAudioMetaData("AA3", "STEREO", true);

    EXPECT_EQ(event.getAudioCodec(), "AA3");
    EXPECT_EQ(event.getAudioMixType(), "STEREO");
    EXPECT_FALSE(event.getAtmosInfo());
}
// Test case for getMediaFormat and setMediaFormat methods
TEST_F(MediaMetadataEventTest, MediaFormatMethodsTest) {
    MediaMetadataEvent event(1000, 1920, 1080, true, true, "DRM Type", 123456.789, 123, session_id, EFFECTIVE_URL);

    // Test setMediaFormat and then getMediaFormat
    event.setMediaFormat("HLS");
    EXPECT_EQ(event.getMediaFormat(), "HLS");

    // Test setMediaFormat again and getMediaFormat
    event.setMediaFormat("DASH");
    EXPECT_EQ(event.getMediaFormat(), "DASH");
}



// Test functions of BitrateChangeEventTest
class BitrateChangeEventTest : public testing::Test {
protected:
    void SetUp() override {
       
        bitrateChangeEvent = new BitrateChangeEvent(1000, 500000, "Bitrate change", 1920, 1080, 30.0, 5000.0, true, 1920, 1080, eVIDEOSCAN_PROGRESSIVE, 16, 9, session_id);
    }

    void TearDown() override {
        delete bitrateChangeEvent;
    }

    BitrateChangeEvent* bitrateChangeEvent;
};

TEST_F(BitrateChangeEventTest, BitrateChangeEventMethodsTest) {
    EXPECT_EQ(bitrateChangeEvent->getTime(), 1000);
    EXPECT_EQ(bitrateChangeEvent->getBitrate(), 500000);
    EXPECT_EQ(bitrateChangeEvent->getDescription(), "Bitrate change");
    EXPECT_EQ(bitrateChangeEvent->getWidth(), 1920);
    EXPECT_EQ(bitrateChangeEvent->getHeight(), 1080);
    EXPECT_EQ(bitrateChangeEvent->getFrameRate(), 30.0);
    EXPECT_EQ(bitrateChangeEvent->getPosition(), 5000.0);
    EXPECT_TRUE(bitrateChangeEvent->getCappedProfileStatus());
    EXPECT_EQ(bitrateChangeEvent->getDisplayWidth(), 1920);
    EXPECT_EQ(bitrateChangeEvent->getDisplayHeight(), 1080);
    EXPECT_EQ(bitrateChangeEvent->getScanType(), eVIDEOSCAN_PROGRESSIVE);
    EXPECT_EQ(bitrateChangeEvent->getAspectRatioWidth(), 16);
    EXPECT_EQ(bitrateChangeEvent->getAspectRatioHeight(), 9);
}
// Test with a negative time value
TEST_F(BitrateChangeEventTest, BitrateChangeEventNegativeTimeTest) {
    BitrateChangeEvent event(-1000, 500000, "Negative time", 1920, 1080, 30.0, 5000.0, true, 1920, 1080, eVIDEOSCAN_PROGRESSIVE, 16, 9, session_id);
    EXPECT_EQ(event.getTime(), -1000);
}

// Test with a zero bitrate value
TEST_F(BitrateChangeEventTest, BitrateChangeEventZeroBitrateTest) {
    BitrateChangeEvent event(1000, 0, "Zero bitrate", 1920, 1080, 30.0, 5000.0, true, 1920, 1080, eVIDEOSCAN_PROGRESSIVE, 16, 9, session_id);
    EXPECT_EQ(event.getBitrate(), 0);
}

// Test with an empty description
TEST_F(BitrateChangeEventTest, BitrateChangeEventEmptyDescriptionTest) {
    BitrateChangeEvent event(1000, 500000, "", 1920, 1080, 30.0, 5000.0, true, 1920, 1080, eVIDEOSCAN_PROGRESSIVE, 16, 9, session_id);
    EXPECT_EQ(event.getDescription(), "");
   
}
// Test functions of TimedMetadataEventTest
class TimedMetadataEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        timedMetadataEvent = new TimedMetadataEvent("Metadata name", "12345", 10.0, 3000.0, "Hello", session_id);
    }

    void TearDown() override {
        delete timedMetadataEvent;
    }

    TimedMetadataEvent* timedMetadataEvent;
};

TEST_F(TimedMetadataEventTest, TimedMetadataEventMethodsTest) {
    EXPECT_EQ(timedMetadataEvent->getName(), "Metadata name");
    EXPECT_EQ(timedMetadataEvent->getId(), "12345");
    EXPECT_EQ(timedMetadataEvent->getTime(), 10.0);
    EXPECT_EQ(timedMetadataEvent->getDuration(), 3000.0);
    EXPECT_EQ(timedMetadataEvent->getContent(), "Hello");
}


// Test functions of BulkTimedMetadataEventTest
class BulkTimedMetadataEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        bulkTimedMetadataEvent = new BulkTimedMetadataEvent("Hello World", session_id);
    }

    void TearDown() override {
        delete bulkTimedMetadataEvent;
    }

    BulkTimedMetadataEvent* bulkTimedMetadataEvent;
};

TEST_F(BulkTimedMetadataEventTest, BulkTimedMetadataEventMethodsTest) {
    EXPECT_EQ(bulkTimedMetadataEvent->getContent(), "Hello World");
}

// Test functions of StateChangedEventTest
class StateChangedEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        stateChangedEvent = new StateChangedEvent(AAMPPlayerState::eSTATE_PLAYING, session_id);
    }

    void TearDown() override {
        delete stateChangedEvent;
    }

    StateChangedEvent* stateChangedEvent;
};

TEST_F(StateChangedEventTest, StateChangedEventMethodsTest) {
    EXPECT_EQ(stateChangedEvent->getState(), AAMPPlayerState::eSTATE_PLAYING);
}

// Test functions of SupportedSpeedsChangedEventTest
class SupportedSpeedsChangedEventTest : public testing::Test {
protected:
    void SetUp() override {
       
        supportedSpeedsChangedEvent = new SupportedSpeedsChangedEvent(session_id);
        supportedSpeedsChangedEvent->addSupportedSpeed(1.0);
        supportedSpeedsChangedEvent->addSupportedSpeed(0.5);
    }

    void TearDown() override {
        delete supportedSpeedsChangedEvent;
    }

    SupportedSpeedsChangedEvent* supportedSpeedsChangedEvent;
};

TEST_F(SupportedSpeedsChangedEventTest, SupportedSpeedsChangedEventMethodsTest) {
    const std::vector<float>& speeds = supportedSpeedsChangedEvent->getSupportedSpeeds();
    
    EXPECT_EQ(speeds.size(), 2);
    EXPECT_FLOAT_EQ(speeds[0], 1.0);
    EXPECT_FLOAT_EQ(speeds[1], 0.5);

    int count = supportedSpeedsChangedEvent->getSupportedSpeedCount();

    EXPECT_EQ(count,2);
}
// Test functions of SeekedEventTest
class SeekedEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        seekedEvent = new SeekedEvent(500.0, session_id); 
    }

    void TearDown() override {
        delete seekedEvent;
    }

    SeekedEvent* seekedEvent;
};

TEST_F(SeekedEventTest, SeekedEventMethodsTest) {
    EXPECT_EQ(seekedEvent->getPosition(), 500.0);
}
// Test functions of TuneProfilingEvent
class TuneProfilingEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        std::string profilingData = "profiling data";
        tuneProfilingEvent = new TuneProfilingEvent(profilingData, session_id);
    }

    void TearDown() override {
        delete tuneProfilingEvent;
    }

    TuneProfilingEvent* tuneProfilingEvent;
};

TEST_F(TuneProfilingEventTest, TuneProfilingEventMethodsTest) {
    const std::string& profilingData = tuneProfilingEvent->getProfilingData();
    EXPECT_EQ(profilingData, "profiling data");
}
// Test functions of BufferingChangedEventTest
class BufferingChangedEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        bufferingChangedEvent = new BufferingChangedEvent(true, session_id); 
    }

    void TearDown() override {
        delete bufferingChangedEvent;
    }

    BufferingChangedEvent* bufferingChangedEvent;
};

TEST_F(BufferingChangedEventTest, BufferingChangedEventMethodsTest) {
    EXPECT_TRUE(bufferingChangedEvent->buffering());
}

// Test fixture for DrmMetaDataEvent
class DrmMetaDataEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        drmMetaDataEvent = new DrmMetaDataEvent(AAMPTuneFailure::AAMP_TUNE_UNTRACKED_DRM_ERROR, "AccessStatus", 200, 0, false, session_id);
    }

    void TearDown() override {
        delete drmMetaDataEvent;
    }

    DrmMetaDataEvent* drmMetaDataEvent;
};

TEST_F(DrmMetaDataEventTest, SetAndGetFailureTest) {
    AAMPTuneFailure failure = AAMPTuneFailure::AAMP_TUNE_UNTRACKED_DRM_ERROR;
    drmMetaDataEvent->setFailure(failure);
    EXPECT_EQ(drmMetaDataEvent->getFailure(), failure);
}
// Test cases for DrmMetaDataEvent methods
TEST_F(DrmMetaDataEventTest, GetterSetterMethodsTest) {

    drmMetaDataEvent->setAccessStatus("AccessStatus");
    EXPECT_EQ(drmMetaDataEvent->getAccessStatus(), "AccessStatus");

    drmMetaDataEvent->setResponseData("ResponseData");
    EXPECT_EQ(drmMetaDataEvent->getResponseData(), "ResponseData");

    drmMetaDataEvent->setAccessStatusValue(400);
    EXPECT_EQ(drmMetaDataEvent->getAccessStatusValue(), 400);

    drmMetaDataEvent->setResponseCode(404);
    EXPECT_EQ(drmMetaDataEvent->getResponseCode(), 404);

    drmMetaDataEvent->setSecManagerReasonCode(100);
    EXPECT_EQ(drmMetaDataEvent->getSecManagerReasonCode(), 100);

    int32_t secManagerClassCode = drmMetaDataEvent->getSecManagerClassCode();
    EXPECT_EQ(secManagerClassCode, -1);  

    int32_t businessStatus = drmMetaDataEvent->getBusinessStatus();
    EXPECT_EQ(businessStatus, -1);  

    drmMetaDataEvent->setSecclientError(true);
    EXPECT_TRUE(drmMetaDataEvent->getSecclientError());

    drmMetaDataEvent->setHeaderResponses({"Header1", "Header2"});
    const std::vector<std::string>& headerResponses = drmMetaDataEvent->getHeaderResponses();
    EXPECT_EQ(headerResponses.size(), 2);
    EXPECT_EQ(headerResponses[0], "Header1");
    EXPECT_EQ(headerResponses[1], "Header2");

    drmMetaDataEvent->setBodyResponse("{\"errorCode\":\"Error1\",\"description\":\"Invalid parameter value: bt\"}");
    const std::string bodyResponses = drmMetaDataEvent->getBodyResponse();
    EXPECT_FALSE(bodyResponses.empty());
    EXPECT_EQ(bodyResponses, "{\"errorCode\":\"Error1\",\"description\":\"Invalid parameter value: bt\"}");
}
// Test case for ConvertToVerboseErrorCode method
TEST_F(DrmMetaDataEventTest, ConvertToVerboseErrorCodeTest) {
    drmMetaDataEvent->ConvertToVerboseErrorCode(404, 0);
    int value = drmMetaDataEvent->getSecManagerClassCode();
    EXPECT_EQ(value, 200);
    EXPECT_EQ(drmMetaDataEvent->getSecManagerReasonCode(), 1);
    EXPECT_EQ(drmMetaDataEvent->getBusinessStatus(), -1);

    drmMetaDataEvent->ConvertToVerboseErrorCode(412, 401);
    EXPECT_EQ(drmMetaDataEvent->getSecManagerReasonCode(), 1);
}

// Test case for SetVerboseErrorCode method
TEST_F(DrmMetaDataEventTest, SetVerboseErrorCodeTest) {
    drmMetaDataEvent->SetVerboseErrorCode(200, 100, 300);
    EXPECT_EQ(drmMetaDataEvent->getSecManagerClassCode(), 200);
    EXPECT_EQ(drmMetaDataEvent->getSecManagerReasonCode(), 100);
    EXPECT_EQ(drmMetaDataEvent->getBusinessStatus(), 300);
}

TEST_F(DrmMetaDataEventTest, SetAndGetNetworkMetricData) {
    const std::string networkMetricData = "SampleNetworkMetricData";

    drmMetaDataEvent->setNetworkMetricData(networkMetricData);

    EXPECT_EQ(drmMetaDataEvent->getNetworkMetricData(), networkMetricData);
}

// Test functions of AnomalyReportEvent
class AnomalyReportEventTest : public testing::Test {
protected:
    void SetUp() override {
       
        anomalyReportEvent = new AnomalyReportEvent(2,"anomaly message", session_id); 
    }

    void TearDown() override {
        delete anomalyReportEvent;
    }

    AnomalyReportEvent* anomalyReportEvent;
};

// Testing AnomalyReportEvent
TEST(AnomalyReportEventTest, AnomalyReportEventMethodsTest) {
    AnomalyReportEvent anomalyReportEvent(2, "anomaly message", session_id);

    EXPECT_EQ(anomalyReportEvent.getSeverity(), 2);
    EXPECT_EQ(anomalyReportEvent.getMessage(), "anomaly message");
}

// Test functions of WebVttCueEvent
class WebVttCueEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        cueData = new VTTCue(0.0,2.5,"cue Text","settings");
        webVttCueEvent = new WebVttCueEvent(cueData, session_id); 
    }

    void TearDown() override {
        delete webVttCueEvent;
        delete cueData;
    }
    VTTCue* cueData;
    WebVttCueEvent* webVttCueEvent;
};

// Testing WebVttCueEvent
TEST_F(WebVttCueEventTest, WebVttCueEventMethodsTest) {
    EXPECT_EQ(webVttCueEvent->getCueData(), cueData);
}
// Test functions of AdResolvedEvent
class AdResolvedEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        adResolvedEvent = new AdResolvedEvent(true, "ad123", 1000, 15000, "", "", session_id);
    }

    void TearDown() override {
        delete adResolvedEvent;
    }

    AdResolvedEvent* adResolvedEvent;
};
// Testing AdResolvedEvent
TEST_F(AdResolvedEventTest, AdResolvedEventMethodsTest) {
    AdResolvedEvent adResolvedEvent(true, "ad123", 1000, 15000, "", "", session_id);

    EXPECT_TRUE(adResolvedEvent.getResolveStatus());
    EXPECT_EQ(adResolvedEvent.getAdId(), "ad123");
    EXPECT_EQ(adResolvedEvent.getStart(), 1000);
    EXPECT_EQ(adResolvedEvent.getDuration(), 15000);
}
// Test functions of AdReservationEvent
class AdReservationEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        adBreakId = "break_id";
        position = 12345;
		absolutePosition = 123456789;
        adReservationEvent = new AdReservationEvent(AAMP_EVENT_AD_RESERVATION_START, adBreakId, position, absolutePosition, session_id);
    }

    void TearDown() override {
        
        delete adReservationEvent;
    }

    AdReservationEvent* adReservationEvent;
    std::string adBreakId;
    uint64_t position;
	uint64_t absolutePosition;
};

// Testing AdReservationEvent
TEST_F(AdReservationEventTest, AdReservationEventMethodsTest) {
    EXPECT_EQ(adReservationEvent->getAdBreakId(), adBreakId);
    EXPECT_EQ(adReservationEvent->getPosition(), position);
}

// Test functions of AdPlacementEvent
class AdPlacementEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        adId = "ad_id";
        position = 12345;
		absolutePosition = 123456789;
        offset = 1000;
        duration = 3000;
        errorCode = 0;
        adPlacementEvent = new AdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_START, adId, position, absolutePosition, session_id, offset, duration, errorCode);
    }

    void TearDown() override {
      
        delete adPlacementEvent;
    }

    AdPlacementEvent* adPlacementEvent;
    std::string adId;
    uint32_t position;
	uint64_t absolutePosition;
    uint32_t offset;
    uint32_t duration;
    int errorCode;
};

// Testing AdPlacementEvent
TEST_F(AdPlacementEventTest, AAMP_EVENT_AD_PLACEMENT_START) {
    EXPECT_EQ(adPlacementEvent->getAdId(), adId);
    EXPECT_EQ(adPlacementEvent->getPosition(), position);
    EXPECT_EQ(adPlacementEvent->getOffset(), offset);
    EXPECT_EQ(adPlacementEvent->getDuration(), duration);
    EXPECT_EQ(adPlacementEvent->getErrorCode(), errorCode);
}
// Test functions of MetricsDataEvent
class MetricsDataEventTest : public testing::Test {
protected:
    void SetUp() override {
       
        dataType = MetricsDataType::AAMP_DATA_NONE;
        metricUUID = "uuid";
        metricsData = "metrics_data";
        metricsDataEvent = new MetricsDataEvent(dataType, metricUUID, metricsData, session_id);
    }

    void TearDown() override {
        
        delete metricsDataEvent;
    }

    MetricsDataEvent* metricsDataEvent;
    MetricsDataType dataType;
    std::string metricUUID;
    std::string metricsData;
};

// Testing MetricsDataEvent
TEST_F(MetricsDataEventTest, MetricsDataEventMethodsTest) {
    EXPECT_EQ(metricsDataEvent->getMetricsDataType(), dataType);
    EXPECT_EQ(metricsDataEvent->getMetricUUID(), metricUUID);
    EXPECT_EQ(metricsDataEvent->getMetricsData(), metricsData);
}
// Test functions of ID3MetadataEvent
class ID3MetadataEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        metadata = {1, 2, 3};
        id = 123;
        timeScale = 90000;
        schemeIdUri = "scheme";
        id3Value = "value";
        eventDuration = 500;
        presentationTime = 123456789;
        timestampOffset = 987654321;
        id3MetadataEvent = new ID3MetadataEvent(metadata, schemeIdUri, id3Value, timeScale, presentationTime, eventDuration, id, timestampOffset, session_id);
    }

    void TearDown() override {
        
        delete id3MetadataEvent;
    }

    ID3MetadataEvent* id3MetadataEvent;
    std::vector<uint8_t> metadata;
    int id;
    uint32_t timeScale;
    std::string schemeIdUri;
    std::string id3Value;
    uint32_t eventDuration;
    uint64_t presentationTime;
    uint64_t timestampOffset;
};

// Testing ID3MetadataEvent
TEST_F(ID3MetadataEventTest, ID3MetadataEventMethodsTest) {
    EXPECT_EQ(id3MetadataEvent->getMetadata(), metadata);
    EXPECT_EQ(id3MetadataEvent->getMetadataSize(), metadata.size());
    EXPECT_EQ(id3MetadataEvent->getTimeScale(), timeScale);
    EXPECT_EQ(id3MetadataEvent->getEventDuration(), eventDuration);
    EXPECT_EQ(id3MetadataEvent->getId(), id);
    EXPECT_EQ(id3MetadataEvent->getPresentationTime(), presentationTime);
    EXPECT_EQ(id3MetadataEvent->getTimestampOffset(), timestampOffset);
    EXPECT_EQ(id3MetadataEvent->getSchemeIdUri(), schemeIdUri);
    EXPECT_EQ(id3MetadataEvent->getValue(), id3Value);
}
// Test functions of DrmMessageEvent
class DrmMessageEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        drmMessage = "DRM message";
        drmMessageEvent = new DrmMessageEvent(drmMessage, session_id);
    }

    void TearDown() override {
        
        delete drmMessageEvent;
    }

    DrmMessageEvent* drmMessageEvent;
    std::string drmMessage;
};
// Testing DrmMessageEvent
TEST_F(DrmMessageEventTest, DrmMessageEventMethodsTest) {
    EXPECT_EQ(drmMessageEvent->getMessage(), drmMessage);
}

class BlockedEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        reason = "Content blocked ";
        locator = "http://example.com/video";
    }

    void TearDown() override {
        // Clean up if needed
    }

    std::string reason;
    std::string locator;
};

TEST_F(BlockedEventTest, ConstructorAndGetters)
{
    BlockedEvent blockedEvent(reason, locator, session_id);

    EXPECT_EQ(blockedEvent.getReason(), reason);
    EXPECT_EQ(blockedEvent.getLocator(), locator);
}

// Test functions of ContentGapEvent
class ContentGapEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        contentGapTime = 123.45;
        contentGapDuration = 67.89;
        contentGapEvent = new ContentGapEvent(contentGapTime, contentGapDuration, session_id);
    }

    void TearDown() override {
        
        delete contentGapEvent;
    }

    ContentGapEvent* contentGapEvent;
    double contentGapTime;
    double contentGapDuration;
};
// Testing ContentGapEvent
TEST_F(ContentGapEventTest, ContentGapEventMethodsTest) {
    EXPECT_EQ(contentGapEvent->getTime(), contentGapTime);
    EXPECT_EQ(contentGapEvent->getDuration(), contentGapDuration);
}
// Test functions of HTTPResponseHeaderEvent
class HTTPResponseHeaderEventTest : public testing::Test {
protected:
    void SetUp() override {
        // Initialize any data for testing
        headerName = "Header_name";
        headerResponse = "Header_Response";
        httpResponseHeaderEvent = new HTTPResponseHeaderEvent(headerName, headerResponse, session_id);
    }

    void TearDown() override {
        
        delete httpResponseHeaderEvent;
    }

    HTTPResponseHeaderEvent* httpResponseHeaderEvent;
    std::string headerName;
    std::string headerResponse;
};
// Testing HTTPResponseHeaderEvent
TEST_F(HTTPResponseHeaderEventTest, HTTPResponseHeaderEventMethodsTest) {
    EXPECT_EQ(httpResponseHeaderEvent->getHeader(), headerName);
    EXPECT_EQ(httpResponseHeaderEvent->getResponse(), headerResponse);
}

class WatermarkSessionUpdateEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        sessionHandle = 123;
        status = 456;
        system = "WatermarkSession";
    }

    void TearDown() override {
        
    }

    uint32_t sessionHandle;
    uint32_t status;
    std::string system;
};

TEST_F(WatermarkSessionUpdateEventTest, WatermarkSessionTestMethod)
{
    
    WatermarkSessionUpdateEvent event(sessionHandle, status, system, session_id);

    
    EXPECT_EQ(event.getSessionHandle(), sessionHandle);
    EXPECT_EQ(event.getStatus(), status);
    EXPECT_EQ(event.getSystem(), system);
}

class ContentProtectionDataEventTest : public testing::Test {
protected:
    void SetUp() override {
        
        keyID = {1,2,3};
        streamType = "Stream";
        contentProtectionDataEvent = new ContentProtectionDataEvent(keyID, streamType, session_id);
    }

    void TearDown() override {
       
        delete contentProtectionDataEvent;
    }

    ContentProtectionDataEvent* contentProtectionDataEvent;
    std::vector<uint8_t> keyID;
    std::string streamType;
};
// Testing ContentProtectionDataEvent
TEST_F(ContentProtectionDataEventTest, ContentProtectionDataEventMethodsTest) {
    EXPECT_EQ(contentProtectionDataEvent->getKeyID(), keyID);
    EXPECT_EQ(contentProtectionDataEvent->getStreamType(), streamType);
}

class ManifestRefreshEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        
        manifestDuration = 100;
        noOfPeriods = 3;
        manifestPublishedTime = 12345;
        manifestType = "dynamic";
        event = new ManifestRefreshEvent(manifestDuration, noOfPeriods, manifestPublishedTime, manifestType, session_id);
    }

    void TearDown() override {
        
        delete event;
    }

    uint32_t manifestDuration;
    int noOfPeriods;
    uint32_t manifestPublishedTime;
    std::string manifestType;
    ManifestRefreshEvent* event;
};

TEST_F(ManifestRefreshEventTest, ConstructorTest) {
    EXPECT_EQ(event->getManifestDuration(), manifestDuration);
    EXPECT_EQ(event->getNoOfPeriods(), noOfPeriods);
    EXPECT_EQ(event->getManifestPublishedTime(), manifestPublishedTime);
    EXPECT_EQ(event->getManifestType(), manifestType);
}

class TuneTimeMetricsEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        
        timeMetricData = "SampleTimeMetricsData";
        event = new TuneTimeMetricsEvent(timeMetricData, session_id);
    }

    void TearDown() override {
        
        delete event;
    }

    TuneTimeMetricsEvent* event;
    std::string timeMetricData;
};

TEST_F(TuneTimeMetricsEventTest, Constructor) {
    EXPECT_EQ(event->getTuneMetricsData(), timeMetricData);
}

TEST_F(TuneTimeMetricsEventTest, GetTuneMetricsData) {
    
    EXPECT_EQ(event->getTuneMetricsData(), timeMetricData);
}

class MonitorAVStatusEventTest : public ::testing::Test {
protected:
	void SetUp() override {
		status = "ok";
		videoPositionMS = 3717;
		audioPositionMS = 3717;
		timeInStateMS = 1748499898430;
		droppedFrames = 0;
		monitorEvent = new MonitorAVStatusEvent(status,videoPositionMS,audioPositionMS,timeInStateMS,session_id,droppedFrames);
	}

	void TearDown() override {
		delete monitorEvent;
	}

	MonitorAVStatusEvent* monitorEvent;
	std::string status;
	int64_t videoPositionMS;
	int64_t audioPositionMS;
	uint64_t timeInStateMS;
	uint64_t droppedFrames;
};

TEST_F(MonitorAVStatusEventTest, ConstructorTest){
	EXPECT_EQ(monitorEvent->getMonitorAVStatus().c_str(),status);
	EXPECT_EQ(monitorEvent->getVideoPositionMS(), videoPositionMS);
	EXPECT_EQ(monitorEvent->getAudioPositionMS(), audioPositionMS);
	EXPECT_EQ(monitorEvent->getTimeInStateMS(), timeInStateMS);
	EXPECT_EQ(monitorEvent->getDroppedFrames(), droppedFrames);
}
