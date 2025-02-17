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
//
// Utils.h
//

/**
 * @file Utils.h
 * @brief
 */

#ifndef FOG_CLI_DASHUTILS_H
#define FOG_CLI_DASHUTILS_H

#include <regex>
#include <map>
#include <regex>
#include <chrono>
#include <iomanip>
#include <iostream>

/**
 * Converts a ISO-8601 duration to seconds.
 */
double isoDurationToSeconds(const std::string &duration, double defaultValue = 0);



/**
 * Converts a ISO-8601 date to seconds since Unix epoch.
 *
 * Note: it currently does not handle time zones and milliseconds.
 * if this is ever required, I strongly recommend using a dedicated
 * library.
 *
 */
double isoDateTimeToEpochSeconds(std::string isotime, double defaultValue = 0);

/**
 * Epoch seconds to iso date time.
 *
 * Note: it currently does not handle time zones and milliseconds.
 * if this is ever required, I strongly recommend using a dedicated
 * library.
 *
 */
bool epochSecondsToIsoDateTime(double seconds, std::string& isoDateTime);

/**
 * Converts seconds to aa ISO-8601 compliant duration.
 * @param seconds
 * @return
 */
std::string epochSecondsToIsoDuration(double seconds);

#endif //FOG_CLI_DASHUTILS_H
