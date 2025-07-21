/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "fragmentcollector_progressive.h"

StreamAbstractionAAMP_PROGRESSIVE::StreamAbstractionAAMP_PROGRESSIVE(class PrivateInstanceAAMP *aamp,double seek_pos, float rate): StreamAbstractionAAMP(aamp)
{
}

StreamAbstractionAAMP_PROGRESSIVE::~StreamAbstractionAAMP_PROGRESSIVE()
{
}

AAMPStatusType StreamAbstractionAAMP_PROGRESSIVE::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_PROGRESSIVE::Start() {  }

void StreamAbstractionAAMP_PROGRESSIVE::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_PROGRESSIVE::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_PROGRESSIVE::GetStreamPosition() { return 0; }

double StreamAbstractionAAMP_PROGRESSIVE::GetFirstPTS() { return 0; }

bool StreamAbstractionAAMP_PROGRESSIVE::IsInitialCachingSupported() { return false; }

long StreamAbstractionAAMP_PROGRESSIVE::GetMaxBitrate()
{ 
    return 0;
}

void StreamAbstractionAAMP_PROGRESSIVE::FetcherLoop()
{

}
bool StreamAbstractionAAMP_PROGRESSIVE::DoEarlyStreamSinkFlush(bool newTune, float rate) { return false; }