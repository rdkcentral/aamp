/*
 * If not stated otherwise in this file or this component's license file the
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

/**
 * @file AampTelemetry2.cpp
 * @brief AAMPTelemetry2 class impl
 */

#include "AampTelemetry2.hpp"
#include <fstream>

#ifndef AAMP_SIMULATOR_BUILD
#include <telemetry_busmessage_sender.h>
#endif


AampTelemetryInitializer::AampTelemetryInitializer()
: m_Initialized(false) // Initialize 'initialized' to false
{
}

void AampTelemetryInitializer::Init()
{
	if(false == m_Initialized)
	{
		m_Initialized = true;
#ifndef AAMP_SIMULATOR_BUILD
		t2_init((char *)"aamp");
		AAMPLOG_MIL("t2_init done ");
#endif
	}
}

bool AampTelemetryInitializer::isInitialized() const
{
	return m_Initialized;
}

AampTelemetryInitializer::~AampTelemetryInitializer()
{
#ifndef AAMP_SIMULATOR_BUILD
	t2_uninit();
	AAMPLOG_MIL("t2_uninit done ");
#endif
}


AampTelemetryInitializer AAMPTelemetry2::mInitializer;

AAMPTelemetry2::AAMPTelemetry2() {
	AAMPTelemetry2("");
}

AAMPTelemetry2::AAMPTelemetry2( const std::string &appName) : appName(appName) {
	
	mInitializer.Init();	// deinit is done in destructor of AampTelemetryInitializer.
}

bool AAMPTelemetry2::send( const std::string &markerName, const std::map<std::string, int>& intData, const std::map<std::string, std::string>& stringData, const std::map<std::string, float>& floatData ) {
	bool bRet = false;
	if(mInitializer.isInitialized()	)
	{
		
		cJSON *root = cJSON_CreateObject();
		
		cJSON_AddStringToObject(root, "app", appName.c_str());
		
		
		for (const auto& pair : intData) {
			std::string key = pair.first;
			int value = pair.second;
			cJSON_AddNumberToObject(root, key.c_str(), value);
		}
		
		for (const auto& pair : stringData) {
			std::string key = pair.first;
			std::string value = pair.second;
			cJSON_AddStringToObject(root, key.c_str(), value.c_str());
		}
		
		for (const auto& pair : floatData) {
			std::string key = pair.first;
			float value = pair.second;
			cJSON_AddNumberToObject(root, key.c_str(), value);
		}
		
		//lets use cJSON_PrintUnformatted , cJSON_Print is formated adds whitespace n hence takes more memory also eats up more logs if logged.
		char *jsonString = cJSON_PrintUnformatted(root);
		
		AAMPLOG_INFO("[M] Marker Name: %s value:%s", markerName.c_str(),jsonString);
		
#ifndef AAMP_SIMULATOR_BUILD
		T2ERROR t2Error = t2_event_s( (char *)markerName.c_str(),jsonString);
		
		if(T2ERROR_SUCCESS == t2Error)
		{
			bRet = true;
		}
		else
		{
			AAMPLOG_ERR("t2_event_s map failed:%d ", t2Error);
		}
#else
		bRet = true;
#endif
		cJSON_free(jsonString);
		cJSON_Delete(root);
	}
	
	return bRet;
}


bool AAMPTelemetry2::send( const std::string &markerName, const char *  data) {
	bool bRet = false;
	if(mInitializer.isInitialized()	&& NULL != data)
	{
		AAMPLOG_INFO("[S] Marker Name: %s value:%s", markerName.c_str(),data );
#ifndef AAMP_SIMULATOR_BUILD
		T2ERROR t2Error =  t2_event_s( (char *)markerName.c_str(),(char*)data );
		
		if(T2ERROR_SUCCESS == t2Error)
		{
			bRet = true;
		}
		else
		{
			AAMPLOG_ERR("t2_event_s string failed:%d ", t2Error);
		}
#else
		bRet = true;
#endif
	}
	
	return bRet;
}
