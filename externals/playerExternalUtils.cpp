/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file playerExternalUtils.cpp
 * @brief Utilities for external's functionalities
 */

#include "playerExternalUtils.h"

#include <sys/stat.h>
//replace below and dependencies with dedicated logger once implemented
#include "AampLogManager.h"

/**
 * Hack to check if code is running in container environment.
 * @return True if running in container environment, false otherwise.
 */
bool IsContainerEnvironment(void)
{
	static bool isContainer;
	static bool isValid;
	if( !isValid )
	{
		struct stat buffer;
		if (stat("/etc/device.properties", &buffer) == 0)
		{ // if we can access file, infer that are are NOT running in container
			AAMPLOG_MIL("not running in container environment");
			isContainer = false;
		}
		else
		{ // if we cannot access file, infer that we ARE running in container
			AAMPLOG_WARN("detected container environment");
			isContainer = true;
		}
		isValid = true;
	}
	return isContainer;
}
