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
 * @file FakeAampMp4Demuxer.cpp
 * @brief Implementation of Fake MP4 Demuxer for testing
 * 
 * This file provides a simple fake implementation of AampMp4Demuxer for basic testing.
 * For more advanced testing scenarios, use the comprehensive fake in test/mocks/.
 */

#include "AampMp4Demuxer.h"
#include "AampLogManager.h"

/**
 * @brief Fake MP4 Demuxer constructor
 */
AampMp4Demuxer::AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type) :
    MediaProcessor(), mMp4Demux(nullptr), mAamp(aamp), mMediaType(type)
{
}

/**
 * @brief Fake MP4 Demuxer destructor
 */
AampMp4Demuxer::~AampMp4Demuxer()
{
}

bool AampMp4Demuxer::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, 
                                 double fragmentPTSoffset, bool discontinuous, bool isInit, 
                                 process_fcn_t processor, bool &ptsError)
{
    ptsError = false;
    return true;
}
