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

#ifndef TIMED_METADATA_H
#define TIMED_METADATA_H

/**
 * @brief Class for Timed Metadata
 */
class TimedMetadata
{
public:
	/**
	 * @brief TimedMetadata Constructor
	 */
	TimedMetadata() : _timeMS(0), _name(""), _content(""), _id(""), _durationMS(0) {}
	
	/**
	 * @brief TimedMetadata Constructor
	 *
	 * @param[in] timeMS - Time in milliseconds
	 * @param[in] name - Metadata name
	 * @param[in] content - Metadata content
	 */
	TimedMetadata(long long timeMS, std::string name, std::string content, std::string id, double durMS) : _timeMS(timeMS), _name(name), _content(content), _id(id), _durationMS(durMS) {}
	
public:
	long long _timeMS;       /**< Time in milliseconds */
	std::string _name;       /**< Metadata name */
	std::string _content;    /**< Metadata content */
	std::string _id;         /**< Id of the timedMetadata. If not available an Id will bre created */
	double _durationMS;      /**< Duration in milliseconds */
};

#endif // TIMED_METADATA_H
