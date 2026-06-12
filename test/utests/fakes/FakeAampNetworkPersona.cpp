/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file FakeAampNetworkPersona.cpp
 * @brief Stub implementation of AampNetworkPersona for unit tests.
 *
 * Always reports IsLoaded() == false so no throttling delays are introduced
 * during test execution.
 */

#include "AampNetworkPersona.h"

AampNetworkPersona::AampNetworkPersona()
    : mMutex(), mLoaded(false), mSequence(), mSequenceStarted(false),
      mSequenceStart(), mRng(0)
{
}

/*static*/
AampNetworkPersona& AampNetworkPersona::Instance()
{
    static AampNetworkPersona sInstance;
    return sInstance;
}

bool AampNetworkPersona::IsLoaded() const
{
    return false;
}

bool AampNetworkPersona::LoadFromFile(const std::string& /*path*/)
{
    return false;
}

double AampNetworkPersona::SampleTtfbMs(bool /*assumeNewConnection*/)
{
    return 0.0;
}

double AampNetworkPersona::SampleTransferMs(std::size_t /*bytes*/)
{
    return 0.0;
}

/*static*/
void AampNetworkPersona::ParsePersonaParams(const std::string& /*json*/, PersonaParams& /*out*/)
{
}

const AampNetworkPersona::PersonaParams& AampNetworkPersona::CurrentParamsLocked() const
{
    static const PersonaParams sDefault;
    return sDefault;
}
