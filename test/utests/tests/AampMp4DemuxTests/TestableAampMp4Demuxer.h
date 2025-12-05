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
 * @file TestableAampMp4Demuxer.h
 * @brief Testable version of AampMp4Demuxer for unit testing
 */

#ifndef __TESTABLE_AAMPMP4DEMUXER_H__
#define __TESTABLE_AAMPMP4DEMUXER_H__

#include "AampMp4Demuxer.h"

#ifdef UNIT_TEST_ENABLED
#include "MockMp4Demux.h"
#include "MockPrivateInstanceAAMP.h"

// Global mock instances
extern MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP;

/**
 * @class TestableAampMp4Demuxer
 * @brief Testable version that allows mock injection
 */
class TestableAampMp4Demuxer : public AampMp4Demuxer
{
public:
    TestableAampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type) 
        : AampMp4Demuxer(aamp, type), mTestEnable(true), mTestAamp(aamp), mTestMediaType(type)
    {
        // Note: Can't access private mMp4Demux, but mock will be used instead
    }

    // Override sendSegment to use mock instead of real Mp4Demux
    bool sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, 
                     double fragmentPTSoffset, bool discontinuous, bool isInit, 
                     process_fcn_t processor, bool &ptsError) override
    {
        bool ret = true;
        (void) processor;
        if (pBuffer && pBuffer->GetLen() && mTestEnable)
        {
            AAMPLOG_WARN("Processing segment with type:%d position: %f, duration: %f, isInit: %d", 
                        mTestMediaType, position, duration, isInit);
            
            // Use mock Mp4Demux
            if (g_mockMp4Demux) {
                g_mockMp4Demux->Parse(pBuffer->GetPtr(), pBuffer->GetLen());
                auto samples = g_mockMp4Demux->GetSamples();
                
                if (samples.size() > 0)
                {
                    for (auto& sample : samples)
                    {
                        AAMPLOG_INFO("Send Stream Transfer for type:%d", mTestMediaType);
                        if (g_mockPrivateInstanceAAMP) {
                            g_mockPrivateInstanceAAMP->SendStreamTransfer(mTestMediaType, sample);
                        }
                    }
                }
                else
                {
                    auto codecInfo = g_mockMp4Demux->GetCodecInfo();
                    AAMPLOG_WARN("[Mp4]Updating codecInfo with format:%d", codecInfo.mCodecFormat);
                    AAMPLOG_INFO("[Mp4]Set Stream Caps for type:%d, format:%d", 
                                mTestMediaType, codecInfo.mCodecFormat);
                    if (g_mockPrivateInstanceAAMP) {
                        g_mockPrivateInstanceAAMP->SetStreamCaps(mTestMediaType, codecInfo);
                    }
                }
            }
        }
        else
        {
            AAMPLOG_WARN("Invalid buffer or demuxer disabled");
        }
        ptsError = false;
        return ret;
    }

    // Public members for testing (avoiding private member access)
    bool mTestEnable;
    PrivateInstanceAAMP* mTestAamp;
    AampMediaType mTestMediaType;
};

#endif // UNIT_TEST_ENABLED

#endif /* __TESTABLE_AAMPMP4DEMUXER_H__ */