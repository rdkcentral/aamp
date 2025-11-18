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
 * @file AampMp4Demuxer.cpp
 * @brief Implementation for MP4 Demuxer
 */

#include "AampMp4Demuxer.h"
#include "AampLogManager.h"

/**
 * @brief MP4 Demuxer constructor
 */
AampMp4Demuxer::AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type) :
    MediaProcessor(), mMp4Demux(nullptr), mAamp(aamp), mEnable(true), mMediaType(type)
{
    AAMPLOG_WARN("Created AampMp4Demuxer(%p) for type %d", this, type);
    mMp4Demux = new Mp4Demux();
}

/**
 * @brief MP4 Demuxer destructor
 */
AampMp4Demuxer::~AampMp4Demuxer()
{
    AAMPLOG_DEBUG("AampMp4Demuxer destructor");
    if (mMp4Demux)
    {
        delete mMp4Demux;
        mMp4Demux = nullptr;
    }
}


/**
 * @fn sendSegment
 *
 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
 * @param[in] position - position of fragment
 * @param[in] duration - duration of fragment
 * @param[in] fragmentPTSoffset - offset PTS of fragment
 * @param[in] discontinuous - true if discontinuous fragment
 * @param[in] isInit - flag for buffer type (init, data)
 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
 * @param[out] ptsError - flag indicates if any PTS error occurred
 * @return true if fragment was sent, false otherwise
 */
bool AampMp4Demuxer::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
                                bool isInit, process_fcn_t processor, bool &ptsError)
{
    bool ret = true;
    (void) processor;
    if (pBuffer && pBuffer->GetLen() && mEnable)
    {
        AAMPLOG_WARN("Processing segment with type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
        mMp4Demux->Parse(pBuffer->GetPtr(), pBuffer->GetLen());
        auto &samples = mMp4Demux->getSamples();
        if (samples.size() > 0)
        {
            for (auto& sample : samples)
            {
                mAamp->SendStreamTransfer(mMediaType, sample);
            }
        }
        else
        {
            const AampCodecInfo& codecInfo = mMp4Demux->getCodecInfo();
            AAMPLOG_WARN("Updating codecInfo with format:%d", codecInfo.mCodecFormat);
            mAamp->SetStreamCaps(mMediaType, codecInfo);
        }
    }
    else
    {
        AAMPLOG_WARN("Invalid buffer or demuxer disabled");
    }
    ptsError = false;
    return ret;
}

const AampCodecInfo& AampMp4Demuxer::getCodecInfo()
{
    return mMp4Demux->getCodecInfo();
}