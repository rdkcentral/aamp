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
 * @file PlayerLogManager.cpp
 * @brief Interface Player logging mechanism source file
 */

#include <thread>
#include <sys/time.h>
#include <ctime>
#include <stdio.h>
#include <cstdarg>
#include <cstring>
#include "PlayerLogManager.h"
#include "PlayerUtils.h"

#ifdef USE_ETHAN_LOG
#include <ethanlog.h>
#else
// stubs for use if USE_ETHAN_LOG not defined
static void vethanlog(int level, const char *filename, const char *function, int line, const char *format, va_list ap){}
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

using namespace std;

static const char *mLogLevelStr[mLOGLEVEL_ERROR+1] =
{
        "TRACE", // mLOGLEVEL_TRACE
        "DEBUG", // mLOGLEVEL_DEBUG
        "INFO",  // mLOGLEVEL_INFO
        "WARN",  // mLOGLEVEL_WARN
        "MIL",   // mLOGLEVEL_MIL
        "ERROR", // mLOGLEVEL_ERROR
};

MW_LogLevel PlayerLogManager::mwLoglevel = mLOGLEVEL_WARN;
bool PlayerLogManager::locked = false;
bool PlayerLogManager::disableLogRedirection = false;
bool PlayerLogManager::enableEthanLogRedirection = false;

static std::hash<std::thread::id> std_thread_hasher;
std::size_t GetPlayerPrintableThreadID( void )
{
	return std_thread_hasher( std::this_thread::get_id() );
}
/**
 * @brief Print logs to console / log file
 */
void logprintf(MW_LogLevel logLevelIndex, const char* file, int line, const char *format, ...)
{
        char timestamp[MW_CLI_TIMESTAMP_PREFIX_MAX_CHARS];
        timestamp[0] = 0x00;
	if( PlayerLogManager::disableLogRedirection )
	{ // add timestamp if not using sd_journal_print
		struct timeval t;
		gettimeofday(&t, NULL);
		snprintf(timestamp, sizeof(timestamp), MW_CLI_TIMESTAMP_PREFIX_FORMAT, (unsigned int)t.tv_sec, (unsigned int)t.tv_usec / 1000 );
	}
	char *format_ptr = NULL;
        int format_bytes = 0;
        for( int pass=0; pass<2; pass++ )
        {
            format_bytes = snprintf(format_ptr, format_bytes,
                                                           "%s[MIDDLEWARE][%s][%zx][%s][%d]%s\n",
                                                           timestamp,
                                                           mLogLevelStr[logLevelIndex],
							   GetPlayerPrintableThreadID(),
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
		    if( PlayerLogManager::disableLogRedirection )
		    { // cli
			    vprintf( format_ptr, args );
		    }
		    else if ( PlayerLogManager::enableEthanLogRedirection )
		    { // remap MW log levels to Ethan log levels
			    int ethanLogLevel;
			    // Important: in production builds, Ethan logger filters out everything
			    // except ETHAN_LOG_MILESTONE and ETHAN_LOG_FATAL
			    switch (logLevelIndex)
			    {
				    case mLOGLEVEL_TRACE:
				    case mLOGLEVEL_DEBUG:
					    ethanLogLevel = ETHAN_LOG_DEBUG;
					    break;

				    case mLOGLEVEL_ERROR:
					    ethanLogLevel = ETHAN_LOG_FATAL;
					    break;

				    case mLOGLEVEL_INFO: // note: we rely on eLOGLEVEL_INFO at tune time for triage
				    case mLOGLEVEL_WARN:
				    case mLOGLEVEL_MIL:
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
void DumpBinaryBlob(const unsigned char *ptr, size_t len)
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

			MW_LOG_WARN("%s", buf);
			dst = buf;
			fit = FIT_CHARS;
		}
	}
}
