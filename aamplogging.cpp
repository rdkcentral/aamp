/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file aamplogging.cpp
 * @brief AAMP logging mechanism source file
 */

#include <iomanip>
#include <algorithm>
#include <thread>
#include <sstream>
#include "priv_aamp.h"
using namespace std;


#ifdef USE_ETHAN_LOG
#include <ethanlog.h>
#else
// stubs for use if USE_ETHAN_LOG not defined
void vethanlog(int level, const char *filename, const char *function, int line, const char *format, va_list ap){}
#define ETHAN_LOG_INFO 0
#define ETHAN_LOG_DEBUG 1
#define ETHAN_LOG_WARNING 2
#define ETHAN_LOG_ERROR 3
#define ETHAN_LOG_FATAL 4
#define ETHAN_LOG_MILESTONE 5
#endif

#ifdef USE_SYSTEMD_JOURNAL_PRINT
#include <systemd/sd-journal.h>
#else
// stub for OSX, where sd_journal_print not available
#define LOG_NOTICE 0
static void sd_journal_printv(int priority, const char *format, va_list arg ){
	size_t fmt_len = strlen(format);
	char *fmt_with_newline = (char *)malloc(fmt_len+2);
	if( fmt_with_newline )
	{
		memcpy( fmt_with_newline, format, fmt_len );
		fmt_with_newline[fmt_len++] = '\n';
		fmt_with_newline[fmt_len++] = 0x00;
		vprintf(fmt_with_newline,arg);
		free( fmt_with_newline );
	}
}
#endif

static const char *mLogLevelStr[eLOGLEVEL_ERROR+1] =
{
	"TRACE", // eLOGLEVEL_TRACE
	"DEBUG", // eLOGLEVEL_DEBUG
	"INFO",  // eLOGLEVEL_INFO
	"WARN",  // eLOGLEVEL_WARN
	"MIL",   // eLOGLEVEL_MIL
	"ERROR", // eLOGLEVEL_ERROR
};

bool AampLogManager::disableLogRedirection = false;
bool AampLogManager::enableEthanLogRedirection = false;
AAMP_LogLevel AampLogManager::aampLoglevel = eLOGLEVEL_WARN;
bool AampLogManager::locked = false;

thread_local int gPlayerId = -1;

/**
 * @brief Print logs to console / log file
 */
void logprintf(AAMP_LogLevel logLevelIndex, const char* file, int line, const char *format, ...)
{
	char timestamp[AAMPCLI_TIMESTAMP_PREFIX_MAX_CHARS];
	timestamp[0] = 0x00;
	if( AampLogManager::disableLogRedirection )
	{ // add timestamp if not using sd_journal_print
		struct timeval t;
		gettimeofday(&t, NULL);
		snprintf(timestamp, sizeof(timestamp), AAMPCLI_TIMESTAMP_PREFIX_FORMAT, (unsigned int)t.tv_sec, (unsigned int)t.tv_usec / 1000 );
	}
	
	char *format_ptr = NULL;
	int format_bytes = 0;
	for( int pass=0; pass<2; pass++ )
	{ // two pass: measure required bytes then populate format string
		format_bytes = snprintf(format_ptr, format_bytes,
							   "%s[AAMP-PLAYER][%d][%s][%zx][%s][%d]%s\n",
							   timestamp,
							   gPlayerId,
							   mLogLevelStr[logLevelIndex],
							   GetPrintableThreadID(),
							   file, line,
							   format );
		if( format_bytes<=0 )
		{ // should never happen!
			break;
		}
		if( pass==0 )
		{
			format_bytes++; // include nul terminator
			format_ptr = (char *)alloca(format_bytes); // allocate on stack
		}
		else
		{
			va_list args;
			va_start(args, format);
			if( AampLogManager::disableLogRedirection )
			{ // aampcli
				vprintf( format_ptr, args );
			}
			else if ( AampLogManager::enableEthanLogRedirection )
			{ // remap AAMP log levels to Ethan log levels
				int ethanLogLevel;
				// Important: in production builds, Ethan logger filters out everything
				// except ETHAN_LOG_MILESTONE and ETHAN_LOG_FATAL
				switch (logLevelIndex)
				{
					case eLOGLEVEL_TRACE:
					case eLOGLEVEL_DEBUG:
						ethanLogLevel = ETHAN_LOG_DEBUG;
						break;
						
					case eLOGLEVEL_ERROR:
						ethanLogLevel = ETHAN_LOG_FATAL;
						break;
						
					case eLOGLEVEL_INFO: // note: we rely on eLOGLEVEL_INFO at tune time for triage
					case eLOGLEVEL_WARN:
					case eLOGLEVEL_MIL:
					default:
						ethanLogLevel = ETHAN_LOG_MILESTONE;
						break;
				}
				vethanlog(ethanLogLevel,NULL,NULL,-1,format_ptr, args);
			}
			else
			{
				format_ptr[format_bytes-1] = 0x00; // strip not-needed newline (good for Ethan Logger, too?)
				sd_journal_printv(LOG_NOTICE,format_ptr,args); // note: truncates to 2040 characters
			}
			va_end(args);
		}
	}
}

/**
 * @brief Compactly log blobs of binary data
 *
 */
void DumpBlob(const unsigned char *ptr, size_t len)
{
#define FIT_CHARS 64
	char buf[FIT_CHARS + 1]; // pad for NUL
	char *dst = buf;
	const unsigned char *fin = ptr+len;
	int fit = FIT_CHARS;
	while (ptr < fin)
	{
		unsigned char c = *ptr++;
		if (c >= ' ' && c < 128)
		{ // printable ascii
			*dst++ = c;
			fit--;
		}
		else if( fit>=4 )
		{
			*dst++ = '[';
			WRITE_HASCII( dst, c );
			*dst++ = ']';
			fit -= 4;
		}
		else
		{
			fit = 0;
		}
		if (fit==0 || ptr==fin )
		{
			*dst++ = 0x00;

			AAMPLOG_WARN("%s", buf);
			dst = buf;
			fit = FIT_CHARS;
		}
	}
}
/**
 * @}
*/
