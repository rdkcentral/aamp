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


#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
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
	 * @brief Sets the externals logger information.
	 *
	 * @param[in] logRedirectStatus Status of log redirection.
	 * @param[in] ethanLogStatus Status of Ethan logging.
	 * @param[in] level Log level.
	 * @param[in] lock Lock status.
	 */	
	static void SetLoggerInfo(bool logRedirectStatus, bool ethanLogStatus, int level, bool lock)
	{
		PlayerLogManager::disableLogRedirection = logRedirectStatus;
		PlayerLogManager::enableEthanLogRedirection = ethanLogStatus;
		PlayerLogManager::setLogLevel(MW_LogLevel(level));
		PlayerLogManager::lockLogLevel(lock);
	}

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
        /**     
         * @fn getHexDebugStr
         */     
        static std::string getHexDebugStr(const std::vector<uint8_t>& data)
        {  
                std::ostringstream hexSs;
                hexSs << "0x";
                hexSs << std::hex << std::uppercase << std::setfill('0');
                std::for_each(data.cbegin(), data.cend(), [&](int c) { hexSs << std::setw(2) << c; });
                return hexSs.str();
        }

};
/**
 * @fn DumpBinaryBlob
 *
 * @param[in] ptr to the buffer
 * @param[in] len length of buffer
 *
 * @return void
 */
void DumpBinaryBlob(const unsigned char *ptr, size_t len);
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

#ifdef MW_LOG_TRACE
// avoid compile time macro redefinition warnings due to defines in SubtecChannel.cpp
#undef MW_LOG_TRACE
#undef MW_LOG_DEBUG
#undef MW_LOG_INFO
#undef MW_LOG_WARN
#undef MW_LOG_MIL
#undef MW_LOG_ERR
#endif

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
