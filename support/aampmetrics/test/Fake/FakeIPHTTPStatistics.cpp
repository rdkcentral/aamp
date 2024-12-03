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
#include "IPHTTPStatistics.h"

void CSessionSummary::IncrementCount(std::string response)
{
}

void CSessionSummary::UpdateSummary(int response, bool connectivity)
{
}

void CLatencyReport::RecordLatency(long timeMs)
{
}

void ManifestGenericStats::UpdateManifestData(ManifestData *manifestData)
{
}

size_t CSessionSummary::totalErrorCount;

cJSON * CLatencyReport::ToJson() const
{
    return nullptr;
}

cJSON * ManifestGenericStats::ToJson() const
{
    return nullptr;
}

cJSON * CSessionSummary::ToJson() const
{
    return nullptr;
}



