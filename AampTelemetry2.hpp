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
 * @file AampTelemetry2.hpp
 * @brief Supporting class to provide telemetry support to AAMP
 */

#ifndef __AAMP_TELEMETRY_2_H__
#define __AAMP_TELEMETRY_2_H__

#include <cjson/cJSON.h>
#include <iostream>
#include <string>
#include <map>
#include "AampLogManager.h"

// Note that RDK telemetry 2.0 support is per process basic, 
// this class is created to take care of un initialization of telemetry but having object as global variable 
// when process goes down, destructor of this class will be called and it will uninitialize the telemetry. 

class AampTelemetryInitializer {
private:
	bool m_Initialized = false;
public:
	AampTelemetryInitializer();
	void Init();
	bool isInitialized() const; 
	~AampTelemetryInitializer();
};


class AAMPTelemetry2 {
private:
	static AampTelemetryInitializer mInitializer;
	std::string appName;
	
public:
	/**
	 * @brief Constructor
	 * @param[in] NONE
	 */
	AAMPTelemetry2();
	
	/**
	 * @brief Constructor
	 * @param[in] appName - Name of the application
	 */
	AAMPTelemetry2(const std::string &appName);
	
	/**
	 *  
	 * @brief send  - Send the telemetry data to the telemetry bus by converting input map to json string
	 * @param[in] markerName - Name of the marker
	 * @param[in] intData - Map of int data
	 * @param[in] stringData - Map of string data
	 * @param[in] floatData - Map of float data
	 * @return bool - true if success, false otherwise
	 */
	bool send(const std::string &markerName, const std::map<std::string, int>& intData, const std::map<std::string, std::string>& stringData, const std::map<std::string, float>& floatData);
	
	/**
	 * @brief send  - Send the telemetry data to the telemetry bus
	 * @param[in] markerName - Name of the marker
	 * @param[in] data - Data to be sent
	 */
	bool send(const std::string &markerName, const char *  data);
};

#endif // __AAMP_TELEMETRY_2_H__
