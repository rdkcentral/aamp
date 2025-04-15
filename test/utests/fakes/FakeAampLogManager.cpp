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
#include "priv_aamp.h"
#include "AampLogManager.h"

// Enable all log levels for L1 testing by default and lock the minimum log level,
// as AAMP changes the log level internally in some scenarios.
// The minimum log level could be changed in a test by calling lockLogLevel(false) and setLogLevel()
#ifndef TEST_LOG_LEVEL
#define TEST_LOG_LEVEL eLOGLEVEL_TRACE
#endif

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

bool AampLogManager::disableLogRedirection = false;
bool AampLogManager::enableEthanLogRedirection = false;
AAMP_LogLevel AampLogManager::aampLoglevel = TEST_LOG_LEVEL;
bool AampLogManager::locked = true;

thread_local int gPlayerId = -1;

void logprintf(AAMP_LogLevel level, const char* file, int line, const char *format, ...)
{
// ENABLE_LOGGING is defined in L1 utests CMakeLists.txt
#ifdef ENABLE_LOGGING
	char *format_ptr = NULL;
	int format_bytes = 0;
	for( int pass=0; pass<2; pass++ )
	{ // two pass: measure required bytes then populate format string
		format_bytes = snprintf(format_ptr, format_bytes,
							   "[AAMP-PLAYER][%d][%s][%zx][%s][%d]%s\n",
							   gPlayerId,
							   mLogLevelStr[level],
							   GetPrintableThreadID(),
							   file, line,
							   format );
		assert( format_bytes>0 );
		if( pass==0 )
		{
			format_bytes++; // include nul terminator
			format_ptr = (char *)alloca(format_bytes); // allocate on stack
		}
		else
		{
			va_list args;
			va_start(args, format);
			vprintf( format_ptr, args );
			va_end(args);
		}
	}
#endif
}

void DumpBlob(const unsigned char *ptr, size_t len)
{
}

#if 0
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
#endif
