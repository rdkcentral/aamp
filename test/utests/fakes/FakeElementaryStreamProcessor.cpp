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

#include "ElementaryProcessor.h"

ElementaryProcessor::ElementaryProcessor(class PrivateInstanceAAMP *aamp)
{
}

bool ElementaryProcessor::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
						                    bool isInit, process_fcn_t processor, bool &ptsError)
{
    return true;
}

void ElementaryProcessor::abort()
{
}

void ElementaryProcessor::reset()
{
}

void ElementaryProcessor::setRate(double rate, PlayMode mode)
{
}

void ElementaryProcessor::abortInjectionWait()
{
}

ElementaryProcessor::~ElementaryProcessor()
{
}
