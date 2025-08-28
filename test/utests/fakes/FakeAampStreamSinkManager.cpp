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

#include "AampStreamSinkManager.h"
#include "MockAampStreamSinkManager.h"
#include "priv_aamp.h"

MockAampStreamSinkManager *g_mockAampStreamSinkManager = nullptr;


AampStreamSinkManager::AampStreamSinkManager()
{
}

AampStreamSinkManager::~AampStreamSinkManager()
{
}

void AampStreamSinkManager::SetSinglePipelineMode(PrivateInstanceAAMP *aamp)
{
}

void AampStreamSinkManager::CreateStreamSink(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames )
{
}

void AampStreamSinkManager::SetStreamSink(PrivateInstanceAAMP *aamp, StreamSink *streamSink)
{
}

void AampStreamSinkManager::DeleteStreamSink(PrivateInstanceAAMP *aamp)
{
}

void AampStreamSinkManager::SetEncryptedHeaders(PrivateInstanceAAMP *aamp, std::map<int, std::string>& mappedHeaders)
{
}

void AampStreamSinkManager::GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders)
{
}

void AampStreamSinkManager::ReinjectEncryptedHeaders()
{
}

void AampStreamSinkManager::DeactivatePlayer(PrivateInstanceAAMP *aamp, bool stop)
{
}

void AampStreamSinkManager::ActivatePlayer(PrivateInstanceAAMP *aamp)
{
}

/**
 * @brief Creates a singleton instance of AampStreamSinkManager
 */
AampStreamSinkManager& AampStreamSinkManager::GetInstance()
{
    if (g_mockAampStreamSinkManager)
    {
        return *g_mockAampStreamSinkManager;
    }
    else
    {
        static AampStreamSinkManager instance;
        return instance;
    }
}

StreamSink* AampStreamSinkManager::GetActiveStreamSink(PrivateInstanceAAMP *aamp)
{
    return nullptr;
}

StreamSink* AampStreamSinkManager::GetStreamSink(PrivateInstanceAAMP *aamp)
{
    return nullptr;
}

StreamSink* AampStreamSinkManager::GetStoppingStreamSink(PrivateInstanceAAMP *aamp)
{
    return nullptr;
}

void AampStreamSinkManager::UpdateTuningPlayer(PrivateInstanceAAMP *aamp)
{
}

void AampStreamSinkManager::AddMediaHeader(unsigned track, std::shared_ptr<AampStreamSinkManager::MediaHeader> header)
{
    if (g_mockAampStreamSinkManager)
    {
	g_mockAampStreamSinkManager->AddMediaHeader(track, header);
    }
}

void AampStreamSinkManager::RemoveMediaHeader(unsigned track)
{
    if (g_mockAampStreamSinkManager)
    {
	g_mockAampStreamSinkManager->RemoveMediaHeader(track);
    }
}

std::shared_ptr<AampStreamSinkManager::MediaHeader> AampStreamSinkManager::GetMediaHeader(unsigned track)
{
    if (g_mockAampStreamSinkManager)
    {
	return g_mockAampStreamSinkManager->GetMediaHeader(track);
    }
	return {};
}

