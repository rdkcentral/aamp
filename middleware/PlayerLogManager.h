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
#ifndef PLAYER_LOG_MANAGER_H
#define PLAYER_LOG_MANAGER_H

/**
 * @file PlayerLogManager.h
 * @brief Log manager for Player Interface
 */

#include <stdio.h>
#include <iostream>
#include <sys/time.h>
#include <ctime>
#include <iomanip>

/**
 * @brief Log level's of Middleware
 */
enum MW_LogLevel
{
        mLOGLEVEL_TRACE,    /**< Trace level */
        mLOGLEVEL_DEBUG,        /**< Debug level */
        mLOGLEVEL_INFO,     /**< Info level */
        mLOGLEVEL_WARN,     /**< Warn level */
        mLOGLEVEL_MIL,      /**< Milestone level */
        mLOGLEVEL_ERROR,    /**< Error level */
};

/**
 * @class PlayerLogManager 
 * @brief PlayerLogManager Class
 */
class PlayerLogManager 
{
public :
	static MW_LogLevel mwLoglevel;
	static bool locked;
	static bool disableLogRedirection;		/**<  disables log re-direction to journal or ethan log apis and uses vprintf - used by simulators */
	static bool enableEthanLogRedirection;  /**<  Enables Ethan log redirection which uses Ethan lib for logging */

	/**
	 * @fn isLogLevelAllowed
	 *
	 * @param[in] chkLevel - log level
	 * @retval true if the log level allowed for print mechanism
	 */
	static bool isLogLevelAllowed(MW_LogLevel chkLevel)
	{
		return (chkLevel>=mwLoglevel);
	}
	
	/**
	 * @fn setLogLevel
	 *
	 * @param[in] newLevel - log level new value
	 * @return void
	 */
	static void setLogLevel(MW_LogLevel newLevel)
	{
		if( !locked )
		{
			mwLoglevel = newLevel;
		}
	}
	
	/**
	 * @brief lock or unlock log level.  This allows (for example) logging to be locked to info or trace, so that "more verbose while tuning, less verbose after tune complete" behavior doesn't override desired log level used for debugging.  This is also used as part of aampcli "noisy" and "quiet" command handling.
	 * 
	 * @param lock if true, subsequent calls to setLogLevel will be ignored
	 */
	static void lockLogLevel( bool lock )
	{
		locked = lock;
	}

};

/**
 * @fn logprintf
 * @param[in] format - printf style string
 * @return void
 */
extern void logprintf(MW_LogLevel logLevelIndex, const char* file, int line, const char *format, ...) __attribute__ ((format (printf, 4, 5)));

#define MW_CLI_TIMESTAMP_PREFIX_MAX_CHARS 20
#define MW_CLI_TIMESTAMP_PREFIX_FORMAT "%u.%03u: "

#define MW_LOG( LEVEL, FORMAT, ... ) \
do{\
if( (LEVEL) >= PlayerLogManager::mwLoglevel ) \
{ \
 logprintf( LEVEL, __FUNCTION__, __LINE__, FORMAT, ##__VA_ARGS__); \
}\
}while(0)

/**
 * @brief Middleware logging defines, this can be enabled through setLogLevel() as per the need
 */
#define MW_LOG_TRACE(FORMAT, ...) MW_LOG(mLOGLEVEL_TRACE, FORMAT, ##__VA_ARGS__)
#define MW_LOG_DEBUG(FORMAT, ...) MW_LOG(mLOGLEVEL_DEBUG, FORMAT, ##__VA_ARGS__)
#define MW_LOG_INFO(FORMAT, ...)  MW_LOG(mLOGLEVEL_INFO, FORMAT, ##__VA_ARGS__)
#define MW_LOG_WARN(FORMAT, ...)  MW_LOG(mLOGLEVEL_WARN, FORMAT, ##__VA_ARGS__)
#define MW_LOG_MIL(FORMAT, ...)   MW_LOG(mLOGLEVEL_MIL, FORMAT, ##__VA_ARGS__)
#define MW_LOG_ERR(FORMAT, ...)   MW_LOG(mLOGLEVEL_ERROR, FORMAT, ##__VA_ARGS__)

#endif /* PLAYER_LOG_MANAGER_H */
