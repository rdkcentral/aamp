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

#include "IPVideoStat.h"
#include "ManifestGenericStats.h"


char * ToJsonString(const char* additionalData = nullptr, bool forPA = false)
{
    return nullptr;
}

cJSON* CHTTPStatistics::ToJson() const
{
    cJSON *monitor = cJSON_CreateObject();
    return monitor;
}

void CHTTPStatistics::IncrementCount(long, int, bool, ManifestData*)
{

}

CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num)
{
    return nullptr;
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    return true;
}

CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
    return nullptr;
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
    return nullptr;
}
