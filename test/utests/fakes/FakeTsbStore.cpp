/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include "TsbApi.h"
#include "MockTSBStore.h"

using namespace TSB;

MockTSBStore *g_mockTSBStore = nullptr;

class TSB::StoreImpl
{
};

Store::Store(const Config& config, LogFunction logger, LogLevel level)
{
}

Store::~Store() = default;

TSB::Status Store::Write(const std::string& url, const void* buffer, std::size_t size)
{
    if (g_mockTSBStore)
    {
        return g_mockTSBStore->Write(url, buffer, size);
    }
    else
    {
        return TSB::Status::FAILED;
    }
}

TSB::Status Store::Read(const std::string& url, void* buffer, std::size_t size) const
{
    if (g_mockTSBStore)
    {
        return g_mockTSBStore->Read(url, buffer, size);
    }
    else
    {
        return TSB::Status::FAILED;
    }
}

std::size_t Store::GetSize(const std::string& url) const
{
    if (g_mockTSBStore)
    {
        return g_mockTSBStore->GetSize(url);
    }
    else
    {
        return 0;
    }
}

void Store::Delete(const std::string& url)
{
    if (g_mockTSBStore)
    {
        g_mockTSBStore->Delete(url);
    }
}

void Store::Flush()
{
    if (g_mockTSBStore)
    {
        g_mockTSBStore->Flush();
    }
}
