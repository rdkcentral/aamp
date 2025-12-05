/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file FunctionalTests.cpp
 * @brief Functional unit tests for AampMp4Demuxer
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>
#include <chrono>

//Google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// unit under test
#include "AampMp4Demuxer.h"
#include "TestableAampMp4Demuxer.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMp4Demux.h"
#include "MockGLib.h"
#include "AampGrowableBuffer.h"
#include "mediaprocessor.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::AnyNumber;
using ::testing::Invoke;

// Global mock instances
MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP;
extern MockGLib *g_mockGLib;  // Defined in FakeGLib.cpp

// Helper functions for GLib memory operations
static gpointer callMalloc(gsize n_bytes)
{
    return malloc(n_bytes);
}

static void callFree(gpointer mem)
{
    free(mem);
}

static gpointer callRealloc(gpointer mem, gsize n_bytes)
{
    return realloc(mem, n_bytes);
}

/**
 * @class AampMp4DemuxerBaseTests
 * @brief Base test fixture for AampMp4Demuxer tests
 */
class AampMp4DemuxerBaseTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create mock instances
        g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
        g_mockMp4Demux = new NiceMock<MockMp4Demux>();
        g_mockGLib = new NiceMock<MockGLib>();
        
        // Set up GLib mock expectations for memory operations
        EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillRepeatedly(Invoke(callMalloc));
        EXPECT_CALL(*g_mockGLib, g_free(_)).WillRepeatedly(Invoke(callFree));
        EXPECT_CALL(*g_mockGLib, g_realloc(_, _)).WillRepeatedly(Invoke(callRealloc));
        
        // Create the demuxer instance with mocked AAMP
        mDemuxer = new AampMp4Demuxer(reinterpret_cast<PrivateInstanceAAMP*>(g_mockPrivateInstanceAAMP), 
                                     eMEDIATYPE_VIDEO);
        
        // Replace the internal Mp4Demux instance with our mock
        // Note: This requires making mMp4Demux accessible for testing
    }

    void TearDown() override
    {
        delete mDemuxer;
        delete g_mockPrivateInstanceAAMP;
        delete g_mockMp4Demux;
        delete g_mockGLib;
        g_mockPrivateInstanceAAMP = nullptr;
        g_mockMp4Demux = nullptr;
        g_mockGLib = nullptr;
    }

    AampMp4Demuxer* mDemuxer;
};

/**
 * @class AampMp4DemuxerTestWithInternalMock
 * @brief Test fixture with injectable Mp4Demux mock
 * 
 * This fixture allows us to inject the mock Mp4Demux instance
 * for more controlled testing
 */
class AampMp4DemuxerMockTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create mock instances
        g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
        g_mockMp4Demux = new NiceMock<MockMp4Demux>();
        g_mockGLib = new NiceMock<MockGLib>();
        
        // Set up default GLib mock behavior for memory operations
        // Using ON_CALL instead of EXPECT_CALL to set default behavior
        ON_CALL(*g_mockGLib, g_malloc(_)).WillByDefault(Invoke(callMalloc));
        ON_CALL(*g_mockGLib, g_free(_)).WillByDefault(Invoke(callFree));
        ON_CALL(*g_mockGLib, g_realloc(_, _)).WillByDefault(Invoke(callRealloc));
        
        // Allow any number of calls
        EXPECT_CALL(*g_mockGLib, g_malloc(_)).Times(AnyNumber());
        EXPECT_CALL(*g_mockGLib, g_free(_)).Times(AnyNumber());
        EXPECT_CALL(*g_mockGLib, g_realloc(_, _)).Times(AnyNumber());
    }

    void TearDown() override
    {
        if (mDemuxer) {
            delete mDemuxer;
        }
        delete g_mockPrivateInstanceAAMP;
        delete g_mockMp4Demux;
        delete g_mockGLib;
        g_mockPrivateInstanceAAMP = nullptr;
        g_mockMp4Demux = nullptr;
        g_mockGLib = nullptr;
    }

    TestableAampMp4Demuxer* mDemuxer = nullptr;
};

/**
 * @brief Test AampMp4Demuxer constructor and destructor
 */
TEST_F(AampMp4DemuxerBaseTests, ConstructorDestructor)
{
    // Constructor creates the object successfully
    EXPECT_NE(mDemuxer, nullptr);
    
    // Test different media types
    AampMp4Demuxer audioDemuxer(reinterpret_cast<PrivateInstanceAAMP*>(g_mockPrivateInstanceAAMP), 
                                eMEDIATYPE_AUDIO);
    EXPECT_TRUE(true); // Constructor should complete without throwing
}

/**
 * @brief Test sendSegment with valid buffer containing samples
 */
TEST_F(AampMp4DemuxerMockTests, SendSegmentWithSamples)
{
    // Create a custom demuxer that uses our mock Mp4Demux
    mDemuxer = new TestableAampMp4Demuxer(reinterpret_cast<PrivateInstanceAAMP*>(g_mockPrivateInstanceAAMP), 
                                         eMEDIATYPE_VIDEO);

    // Debug: Verify mock is set up
    ASSERT_NE(g_mockGLib, nullptr) << "GLib mock should be initialized";
    
    // Test that g_realloc mock is working
    void* testPtr = g_realloc(nullptr, 100);
    ASSERT_NE(testPtr, nullptr) << "g_realloc should work through mock";
    g_free(testPtr);
    
    // Create test buffer - use ReserveBytes then manual data copy to avoid AppendBytes issues
    AampGrowableBuffer buffer("testBuffer");
    
    // Minimal ftyp+mdat MP4 fragment
    const uint8_t minimalMp4[] = {
        // ftyp box (24 bytes)
        0x00, 0x00, 0x00, 0x18,  // box size = 24
        'f', 't', 'y', 'p',       // box type = ftyp
        'i', 's', 'o', 'm',       // major brand = isom
        0x00, 0x00, 0x02, 0x00,  // minor version = 512
        'i', 's', 'o', 'm',       // compatible brand
        'i', 's', 'o', '2',       // compatible brand
        // mdat box (16 bytes)
        0x00, 0x00, 0x00, 0x10,  // box size = 16
        'm', 'd', 'a', 't',       // box type = mdat
        0x00, 0x01, 0x02, 0x03,  // sample data
        0x04, 0x05, 0x06, 0x07   // sample data
    };
    
    // Use ReserveBytes instead of AppendBytes to pre-allocate
    buffer.ReserveBytes(sizeof(minimalMp4));
    buffer.AppendBytes(reinterpret_cast<const char*>(minimalMp4), sizeof(minimalMp4));

    // Create mock media samples
    // Note: Using default-constructed samples to avoid memory allocation issues in test
    // Note: Must use Invoke to construct and return samples since AampMediaSample is move-only
    
    // Set expectations for Mp4Demux mock
    EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
        .Times(1);
    
    EXPECT_CALL(*g_mockMp4Demux, GetSamples())
        .WillOnce(Invoke([]() {
            std::vector<AampMediaSample> mockSamples;
            AampMediaSample sample1, sample2;
            
            // Set only the timing information, not the buffer data
            sample1.mPts = 1000;
            sample1.mDuration = 100;
            
            sample2.mPts = 1100;
            sample2.mDuration = 100;
            
            mockSamples.push_back(std::move(sample1));
            mockSamples.push_back(std::move(sample2));
            
            return mockSamples;
        }));

    // Set expectations for PrivateInstanceAAMP mock
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _))
        .Times(2); // Should be called for each sample

    // Test parameters
    double position = 10.0;
    double duration = 5.0;
    double fragmentPTSoffset = 0.0;
    bool discontinuous = false;
    bool isInit = false;
    MediaProcessor::process_fcn_t processor = nullptr;
    bool ptsError = false;

    // Call sendSegment
    bool result = mDemuxer->sendSegment(&buffer, position, duration, fragmentPTSoffset, 
                                       discontinuous, isInit, processor, ptsError);

    // Verify results
    EXPECT_TRUE(result);
    EXPECT_FALSE(ptsError);
}
/**
 * @brief Test sendSegment with empty buffer
 */
TEST_F(AampMp4DemuxerBaseTests, SendSegmentWithEmptyBuffer)
{
    AampGrowableBuffer emptyBuffer("emptyBuffer");
    bool ptsError = false;
    
    // Verify no calls were made to the mocked dependencies
    EXPECT_CALL(*g_mockMp4Demux, Parse(_, _))
        .Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _))
        .Times(0);
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamCaps(_, _))
        .Times(0);

    // Call sendSegment with empty buffer
    bool result = mDemuxer->sendSegment(&emptyBuffer, 0.0, 0.0, 0.0, false, false, nullptr, ptsError);

     // Should return true but not process anything
    EXPECT_TRUE(result);
    EXPECT_FALSE(ptsError);
}
/**
 * @brief Test sendSegment with different media types
 */
TEST_F(AampMp4DemuxerMockTests, SendSegmentDifferentMediaTypes)
{
    // Test with video
    {
        mDemuxer = new TestableAampMp4Demuxer(reinterpret_cast<PrivateInstanceAAMP*>(g_mockPrivateInstanceAAMP), 
                                             eMEDIATYPE_VIDEO);

        AampGrowableBuffer buffer("videoBuffer");
        buffer.AppendBytes("video_data", 10);

        EXPECT_CALL(*g_mockMp4Demux, Parse(_, _));
        EXPECT_CALL(*g_mockMp4Demux, GetSamples())
            .WillOnce(Invoke([]() {
                std::vector<AampMediaSample> samples;
                AampMediaSample sample;
                sample.mData.AppendBytes("video_sample", 12);
                samples.push_back(std::move(sample));
                return samples;
            }));
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _));

        bool ptsError = false;
        bool result = mDemuxer->sendSegment(&buffer, 1.0, 1.0, 0.0, false, false, nullptr, ptsError);
        
        EXPECT_TRUE(result);
        
        delete mDemuxer;
        mDemuxer = nullptr;
    }

    // Test with subtitle
    {
        mDemuxer = new TestableAampMp4Demuxer(reinterpret_cast<PrivateInstanceAAMP*>(g_mockPrivateInstanceAAMP), 
                                             eMEDIATYPE_SUBTITLE);

        AampGrowableBuffer buffer("subtitleBuffer");
        buffer.AppendBytes("subtitle_data", 13);

        EXPECT_CALL(*g_mockMp4Demux, Parse(_, _));
        EXPECT_CALL(*g_mockMp4Demux, GetSamples())
            .WillOnce(Invoke([]() {
                std::vector<AampMediaSample> samples;
                AampMediaSample sample;
                sample.mData.AppendBytes("subtitle_sample", 15);
                samples.push_back(std::move(sample));
                return samples;
            }));
        EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_SUBTITLE, _));

        bool ptsError = false;
        bool result = mDemuxer->sendSegment(&buffer, 2.0, 1.5, 0.0, false, false, nullptr, ptsError);
        
        EXPECT_TRUE(result);
    } 
}
