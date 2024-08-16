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

#include <cstdarg>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <memory>

#include "MockAampLogManager.h"
#include "priv_aamp.h"
#include "AampLogManager.h"

//Enable the define below to get AAMP logging out when running tests
//#define ENABLE_LOGGING
#define TEST_LOG_LEVEL eLOGLEVEL_TRACE

std::shared_ptr<MockAampLogManager> g_mockAampLogManager = nullptr;

/**
 * @brief Log file and cfg directory path - To support dynamic directory configuration
 */
static char gAampLog[] = "./aamp.log";

bool AampLogManager::isLogLevelAllowed(AAMP_LogLevel chkLevel)
{
	return chkLevel >= TEST_LOG_LEVEL;
}

std::string AampLogManager::getHexDebugStr(const std::vector<uint8_t>& data)
{
	std::ostringstream hexSs;
	hexSs << "0x";
	hexSs << std::hex << std::uppercase << std::setfill('0');
	std::for_each(data.cbegin(), data.cend(), [&](int c) { hexSs << std::setw(2) << c; });
	return hexSs.str();
}

void AampLogManager::setLogLevel(AAMP_LogLevel newLevel)
{
	if (g_mockAampLogManager != nullptr)
	{
		g_mockAampLogManager->setLogLevel(newLevel);
	}
}

static const char *mLogLevelStr[] =
{
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"MIL",
	"ERROR",
	"FATAL"
};

void logprintf(int playerId, AAMP_LogLevel level, const char* file, int line, const char *format, ...)
{
#ifdef ENABLE_LOGGING
	va_list args;
	va_start(args, format);

	char gDebugPrintBuffer[MAX_DEBUG_LOG_BUFF_SIZE];
	int len = snprintf(gDebugPrintBuffer, sizeof(gDebugPrintBuffer), "[AAMP-PLAYER][%d][%s][%s][%d]", playerId, mLogLevelStr[level], file, line);
	vsnprintf(&gDebugPrintBuffer[len], MAX_DEBUG_LOG_BUFF_SIZE-len, format, args);

	std::cout << gDebugPrintBuffer << std::endl;
	std::ofstream f;
	f.open(gAampLog, std::ios::app);
	f << gDebugPrintBuffer << std::endl;
	f.close();
	va_end(args);
#endif
}

void DumpBlob(const unsigned char *ptr, size_t len)
{
}

/**
 *  @brief Print the network error level logging for triage purpose
 */
void AampLogManager::LogNetworkError(const char* url, AAMPNetworkErrorType errorType, int errorCode, AampMediaType type)
{
}

/**
 *  @brief Print the network latency level logging for triage purpose
 */
void AampLogManager::LogNetworkLatency(const char* url, int downloadTime, int downloadThresholdTimeoutMs, AampMediaType type)
{
}

/**
 *  @brief Check curl error before log on console
 */
bool AampLogManager::isLogworthyErrorCode(int errorCode)
{
	if(g_mockAampLogManager)
	{
		return (g_mockAampLogManager->isLogworthyErrorCode(errorCode));
	}
	return false;
}

void AampLogManager::LogABRInfo(AAMPAbrInfo *pstAbrInfo)
{
}

void AampLogManager::aampLogger(std::string &&tsbMessage)
{
}
