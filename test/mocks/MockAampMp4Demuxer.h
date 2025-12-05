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
 * @file MockAampMp4Demuxer.h
 * @brief Mock implementation of AampMp4Demuxer for testing
 */

#ifndef __MOCK_AAMP_MP4_DEMUXER_H__
#define __MOCK_AAMP_MP4_DEMUXER_H__

#include <gmock/gmock.h>
#include "mediaprocessor.h"
#include "AampMediaType.h"
#include "AampGrowableBuffer.h"

/**
 * @class MockAampMp4Demuxer
 * @brief Mock implementation of AampMp4Demuxer using Google Mock
 */
class MockAampMp4Demuxer : public MediaProcessor
{
public:
    MockAampMp4Demuxer() = default;
    virtual ~MockAampMp4Demuxer() = default;

    // Prevent copy construction and assignment
    MockAampMp4Demuxer(const MockAampMp4Demuxer&) = delete;
    MockAampMp4Demuxer& operator=(const MockAampMp4Demuxer&) = delete;

    // Mock all pure virtual methods from MediaProcessor
    MOCK_METHOD(double, getFirstPts, (AampGrowableBuffer* pBuffer), (override));
    MOCK_METHOD(void, setPtsOffset, (double ptsOffset), (override));
    MOCK_METHOD(bool, sendSegment, (AampGrowableBuffer* pBuffer, double position, double duration, 
                                   double fragmentPTSoffset, bool discontinuous, bool isInit, 
                                   process_fcn_t processor, bool& ptsError), (override));
    MOCK_METHOD(void, setRate, (double rate, PlayMode mode), (override));
    MOCK_METHOD(void, setThrottleEnable, (bool enable), (override));
    MOCK_METHOD(void, setFrameRateForTM, (int frameRate), (override));
    MOCK_METHOD(void, abort, (), (override));
    MOCK_METHOD(void, reset, (), (override));
    MOCK_METHOD(void, abortInjectionWait, (), (override));
    MOCK_METHOD(void, enable, (bool enable), (override));
    MOCK_METHOD(void, setTrackOffset, (double offset), (override));

    // Mock virtual methods that have default implementations
    MOCK_METHOD(void, resetPTSOnSubtitleSwitch, (AampGrowableBuffer* pBuffer, double position), (override));
    MOCK_METHOD(void, resetPTSOnAudioSwitch, (AampGrowableBuffer* pBuffer, double position), (override));
    MOCK_METHOD(void, ChangeMuxedAudioTrack, (unsigned char index), (override));
    MOCK_METHOD(void, SetAudioGroupId, (std::string& id), (override));
    MOCK_METHOD(void, setApplyOffsetFlag, (bool enable), (override));
    MOCK_METHOD(void, updateSkipPoint, (double skipPoint, double skipDuration), (override));
    MOCK_METHOD(void, setDiscontinuityState, (bool isDiscontinuity), (override));
    MOCK_METHOD(void, abortWaitForVideoPTS, (), (override));

    // Helper methods for test setup
    void SetupDefaultBehavior() {
        // Set up default return values
        ON_CALL(*this, getFirstPts(::testing::_))
            .WillByDefault(::testing::Return(0.0));
        
        ON_CALL(*this, sendSegment(::testing::_, ::testing::_, ::testing::_, 
                                  ::testing::_, ::testing::_, ::testing::_, 
                                  ::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(
                ::testing::SetArgReferee<7>(false), // Set ptsError to false
                ::testing::Return(true)
            ));

        // Set up void methods to do nothing by default
        ON_CALL(*this, setPtsOffset(::testing::_))
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, setRate(::testing::_, ::testing::_))
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, setThrottleEnable(::testing::_))
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, setFrameRateForTM(::testing::_))
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, abort())
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, reset())
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, abortInjectionWait())
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, enable(::testing::_))
            .WillByDefault(::testing::Return());
        
        ON_CALL(*this, setTrackOffset(::testing::_))
            .WillByDefault(::testing::Return());
    }

    // Test utilities for creating mock buffers
    static AampGrowableBuffer* CreateMockBuffer(const std::string& data) {
        AampGrowableBuffer* buffer = new AampGrowableBuffer("MockBuffer");
        buffer->AppendBytes(data.c_str(), data.length());
        return buffer;
    }

    static AampGrowableBuffer* CreateMockMp4Buffer() {
        // Create a minimal MP4 buffer with ftyp box
        const uint8_t mp4Data[] = {
            0x00, 0x00, 0x00, 0x20, // size = 32
            0x66, 0x74, 0x79, 0x70, // 'ftyp'
            0x69, 0x73, 0x6f, 0x6d, // major_brand = 'isom'
            0x00, 0x00, 0x02, 0x00, // minor_version = 512
            0x69, 0x73, 0x6f, 0x6d, // compatible_brands[0] = 'isom'
            0x69, 0x73, 0x6f, 0x32, // compatible_brands[1] = 'iso2'
            0x61, 0x76, 0x63, 0x31, // compatible_brands[2] = 'avc1'
            0x6d, 0x70, 0x34, 0x31  // compatible_brands[3] = 'mp41'
        };
        
        AampGrowableBuffer* buffer = new AampGrowableBuffer("MockMp4Buffer");
        buffer->AppendBytes((const char*)mp4Data, sizeof(mp4Data));
        return buffer;
    }

    static AampGrowableBuffer* CreateMockInitSegment() {
        return CreateMockMp4Buffer();
    }

    static AampGrowableBuffer* CreateMockDataSegment() {
        // Create a minimal MP4 fragment with moof and mdat
        const uint8_t fragmentData[] = {
            // moof box header
            0x00, 0x00, 0x00, 0x10, // size = 16
            0x6d, 0x6f, 0x6f, 0x66, // 'moof'
            // mfhd box
            0x00, 0x00, 0x00, 0x08, // size = 8
            0x6d, 0x66, 0x68, 0x64, // 'mfhd'
            // mdat box header
            0x00, 0x00, 0x00, 0x08, // size = 8
            0x6d, 0x64, 0x61, 0x74  // 'mdat'
        };
        
        AampGrowableBuffer* buffer = new AampGrowableBuffer("MockFragmentBuffer");
        buffer->AppendBytes((const char*)fragmentData, sizeof(fragmentData));
        return buffer;
    }
};

#endif /* __MOCK_AAMP_MP4_DEMUXER_H__ */