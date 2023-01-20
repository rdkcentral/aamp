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
#include <string>
#include "main_aamp.h"
#include "AampLogManager.h"
#include "tsprocessor.h"

TSProcessor::TSProcessor(AampLogManager *logObj, class PrivateInstanceAAMP *aamp,StreamOperation streamOperation, int track, TSProcessor* peerTSProcessor, TSProcessor* auxTSProcessor)
{
}

TSProcessor::~TSProcessor()
{
}

bool TSProcessor::sendSegment( char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError)
{
    return true;
}

void TSProcessor::setRate(double rate, PlayMode mode)
{
}

void TSProcessor::setThrottleEnable(bool enable)
{
}

void TSProcessor::abort()
{
}

void TSProcessor::reset()
{
}

void TSProcessor::ChangeMuxedAudioTrack(unsigned char index)
{
}

void TSProcessor::SetAudioGroupId(std::string& id)
{
}

void TSProcessor::setApplyOffsetFlag(bool enable)
{
}


