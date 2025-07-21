/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include "isobmffprocessor.h"
#include "MockIsoBmffProcessor.h"

MockIsoBmffProcessor* g_mockIsoBmffProcessor = nullptr;

IsoBmffProcessor::IsoBmffProcessor(class PrivateInstanceAAMP *aamp, id3_callback_t id3_hdl, IsoBmffProcessorType trackType, IsoBmffProcessor* peerBmffProcessor, IsoBmffProcessor* peerSubProcessor)
{
}

bool IsoBmffProcessor::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
						                bool isInit, process_fcn_t processor, bool &ptsError)
{
    return true;
}

void IsoBmffProcessor::abort()
{
}

void IsoBmffProcessor::reset()
{
}

void IsoBmffProcessor::setRate(double rate, PlayMode mode)
{
    if (g_mockIsoBmffProcessor)
    {
        g_mockIsoBmffProcessor->setRate(rate, mode);
    }
}

void IsoBmffProcessor::abortInjectionWait()
{
}

IsoBmffProcessor::~IsoBmffProcessor()
{
}

void IsoBmffProcessor::setPeerSubtitleProcessor(IsoBmffProcessor *processor)
{
}

void IsoBmffProcessor::addPeerListener(MediaProcessor *processor)
{
}
void IsoBmffProcessor::initProcessorForRestamp()
{
}

void IsoBmffProcessor::resetPTSOnAudioSwitch(AampGrowableBuffer *pBuffer, double position)
{
}

void IsoBmffProcessor::updateSkipPoint(double skipPoint, double skipDuration)
{
}
void IsoBmffProcessor::setDiscontinuityState(bool isDiscontinuity)
{
}
void IsoBmffProcessor::waitForVideoPTS()
{
}
void IsoBmffProcessor::abortWaitForVideoPTS()
{
}
void IsoBmffProcessor::resetPTSOnSubtitleSwitch(AampGrowableBuffer *pBuffer, double position)
{
}
