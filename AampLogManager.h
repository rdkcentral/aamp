/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#ifndef AAMPLOGMANAGER_H
#define AAMPLOGMANAGER_H

/**
 * @file AampLogManager.h
 * @brief Log managed for Aamp
 */

#include <vector>
#include <string>
#include <memory.h>
#include <cstdint> // for std::uint8_t
#include "AampMediaType.h"
#include <curl/curl.h>
#include <iomanip> // std::setfill
#include <sstream> // std::ostringstream
#include <algorithm> // std::foreach

extern const char* GetMediaTypeName( AampMediaType mediaType ); // from AampUtils.h; including that directly brings too many other dependencies

/**
 * @brief maximum supported mediatype for latency logging
 */
#define MAX_SUPPORTED_LATENCY_LOGGING_TYPES	(eMEDIATYPE_IFRAME+1)

/**
 * @brief Log level's of AAMP
 */
enum AAMP_LogLevel
{
	eLOGLEVEL_TRACE,    /**< Trace level */
	eLOGLEVEL_DEBUG,	/**< Debug level */
	eLOGLEVEL_INFO,     /**< Info level */
	eLOGLEVEL_WARN,     /**< Warn level */
	eLOGLEVEL_MIL,      /**< Milestone level */
	eLOGLEVEL_ERROR,    /**< Error level */
};

/**
 * @brief Log level network error enum
 */
enum AAMPNetworkErrorType
{
	/* 0 */ AAMPNetworkErrorNone,     /**< No network Error */
	/* 1 */ AAMPNetworkErrorHttp,     /**< HTTP error */
	/* 2 */ AAMPNetworkErrorTimeout,  /**< Timeout Error */
	/* 3 */ AAMPNetworkErrorCurl      /**< curl Error */
};

/**
 * @brief ABR type enum
 */
enum AAMPAbrType
{
	/* 0 */ AAMPAbrBandwidthUpdate,
	/* 1 */ AAMPAbrManifestDownloadFailed,
	/* 2 */ AAMPAbrFragmentDownloadFailed,
	/* 3 */ AAMPAbrUnifiedVideoEngine
};


/**
 * @brief ABR info structure
 */
struct AAMPAbrInfo
{
	AAMPAbrType abrCalledFor;
	int currentProfileIndex;
	int desiredProfileIndex;
	long currentBandwidth;
	long desiredBandwidth;
	long networkBandwidth;
	AAMPNetworkErrorType errorType;
	int errorCode;
};

/**
 * @fn logprintf
 * @param[in] format - printf style string
 * @return void
 */
extern void logprintf(AAMP_LogLevel level, const char* file, int line,const char *format, ...)  __attribute__ ((format (printf, 4, 5)));

/**
 * @class AampLogManager
 * @brief AampLogManager Class
 */
class AampLogManager
{
public:
	static bool disableLogRedirection;		/**<  disables log re-direction to journal or ethan log apis and uses vprintf - used by simulators */
	static AAMP_LogLevel aampLoglevel;
	static bool locked;
	static bool enableEthanLogRedirection;  /**<  Enables Ethan log redirection which uses Ethan lib for logging */
	
	/**
	 * @fn aampLogger
	 *
	 * @param[in] tsbMessage - Message to print
	 * @return void
	 */
	static void aampLogger(std::string &&tsbMessage)
	{
		logprintf(eLOGLEVEL_WARN , __FUNCTION__, __LINE__, "%s", tsbMessage.c_str());
	}
	
	/**
	 * @fn LogNetworkLatency
	 *
	 * @param[in] url - content url
	 * @param[in] downloadTime - download time of the fragment or manifest
	 * @param[in] downloadThresholdTimeoutMs - specified download threshold time out value
	 * @param[in] type - media type
	 * @return void
	 */
	static void LogNetworkLatency(const char* url, int downloadTime, int downloadThresholdTimeoutMs, AampMediaType type)
	{
		std::string location;
		std::string symptom;
		ParseContentUrl(url, location, symptom, type);
		logprintf( eLOGLEVEL_WARN, __FUNCTION__, __LINE__, "AAMPLogNetworkLatency downloadTime=%d downloadThreshold=%d type='%s' location='%s' symptom='%s' url='%s'",
				  downloadTime, downloadThresholdTimeoutMs, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url);
	}
	
	/**
	 * @fn LogNetworkError
	 *
	 * @param[in] url - content url
	 * @param[in] errorType - it can be http or curl errors
	 * @param[in] errorCode - it can be http error or curl error code
	 * @param[in] type - media type
	 * @return void
	 */
	static void LogNetworkError(const char* url, AAMPNetworkErrorType errorType, int errorCode, AampMediaType type)
	{
		std::string location;
		std::string symptom;
		ParseContentUrl(url, location, symptom, type);
		
		switch(errorType)
		{
			case AAMPNetworkErrorHttp:
			{
				if(errorCode >= 400)
				{
					logprintf( eLOGLEVEL_ERROR, __FUNCTION__, __LINE__, "AAMPLogNetworkError error='http error %d' type='%s' location='%s' symptom='%s' url='%s'",
							  errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
				}
			}
				break; /*AAMPNetworkErrorHttp*/
				
			case AAMPNetworkErrorTimeout:
			{
				if(errorCode > 0)
				{
					logprintf( eLOGLEVEL_ERROR, __FUNCTION__, __LINE__, "AAMPLogNetworkError error='timeout %d' type='%s' location='%s' symptom='%s' url='%s'",
							  errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
				}
			}
				break; /*AAMPNetworkErrorTimeout*/
				
			case AAMPNetworkErrorCurl:
			{
				if(errorCode > 0)
				{
					logprintf( eLOGLEVEL_ERROR, __FUNCTION__, __LINE__, "AAMPLogNetworkError error='curl error %d' type='%s' location='%s' symptom='%s' url='%s'",
							  errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
				}
			}
				break; /*AAMPNetworkErrorCurl*/
				
			case AAMPNetworkErrorNone:
				break;
		}
	}
	
	/**
	 * @fn ParseContentUrl
	 *
	 * @param[in] url - content url
	 * @param[out] contentType - it could be a manifest or other audio/video/iframe tracks
	 * @param[out] location - server location
	 * @param[out] symptom - issue exhibiting scenario for error case
	 * @param[in] type - media type
	 * @return void
	 */
	static void ParseContentUrl(const char* url, std::string& location, std::string& symptom, AampMediaType type)
	{
		switch (type)
		{
			case eMEDIATYPE_MANIFEST:
				symptom = "video fails to start, has delayed start or freezes/buffers";
				break;
				
			case eMEDIATYPE_PLAYLIST_VIDEO:
			case eMEDIATYPE_PLAYLIST_AUDIO:
			case eMEDIATYPE_PLAYLIST_IFRAME:
				symptom = "video fails to start or freeze/buffering";
				break;
				
			case eMEDIATYPE_INIT_VIDEO:
			case eMEDIATYPE_INIT_AUDIO:
			case eMEDIATYPE_INIT_IFRAME:
				symptom = "video fails to start";
				break;
				
			case eMEDIATYPE_VIDEO:
				symptom = "freeze/buffering";
				break;
				
			case eMEDIATYPE_AUDIO:
				symptom = "audio drop or freeze/buffering";
				break;
				
			case eMEDIATYPE_IFRAME:
				symptom = "trickplay ends or freezes";
				break;
				
			default:
				symptom = "unknown";
				break;
		}
		
		if(strstr(url,"//mm."))
		{
			location = "manifest manipulator";
		}
		else if(strstr(url,"//odol"))
		{
			location = "edge cache";
		}
		else if(strstr(url,"127.0.0.1:9080"))
		{
			location = "fog";
		}
		else
		{
			location = "unknown";
		}
	}
	
	/**
	 * @fn LogDRMError
	 *
	 * @param[in] major - drm major error code
	 * @param[in] minor - drm minor error code
	 * @return void
	 */
	static void LogDRMError(int major, int minor)
	{
		std::string description;
		switch(major)
		{
			case 3307: /* Internal errors */
				if(minor == 268435462)
				{
					description = "Missing drm keys. Files are missing from /opt/drm. This could happen if socprovisioning fails to pull keys from fkps. This could also happen with a new box type that isn't registered with fkps. Check the /opt/logs/socprov.log for error. Contact ComSec for help.";
				}
				else if(minor == 570425352)
				{
					description = "Stale cache data. There is bad data in adobe cache at /opt/persistent/adobe. This can happen if the cache isn't cleared by /lib/rdk/cleanAdobe.sh after either an FKPS key update or a firmware update. This should not be happening in the field. For engineers, they can try a factory reset to fix the problem.";
				}
				else if(minor == 1000022)
				{
					description = "Local cache directory not readable. The Receiver running as non-root cannot access and read the adobe cache at /opt/persistent/adobe. This can happen if /lib/rdk/prepareChrootEnv.sh fails to set that folders privileges. Run ls -l /opt/persistent and check the access rights. Contact the SI team for help.";
				}
				break; /* 3307 */
				
			case 3321: /* Individualization errors */
				if(minor == 102)
				{
					description = "Invalid signature request on the Adobe individualization request. Expired certs can cause this, so the first course of action is to verify if the certs, temp baked in or production fkps, have not expired.";
				}
				else if(minor == 10100)
				{
					description = "Unknown Device class error from the Adobe individualization server. The drm certs may be been distributed to MSO security team for inclusion in fkps, but Adobe has not yet added the device info to their indi server.";
				}
				else if(minor == 1107296357)
				{
					description = "Failed to connect to individualization server. This can happen if the network goes down. This can also happen if bad proxy settings exist in /opt/xreproxy.conf. Check the receiver.log for the last HttpRequestBegin before the error occurs and check the host name in the url, then check your proxy conf";
				}
				if(minor == 1000595) /* RequiredSPINotAvailable */
				{
					/* This error doesn't tell us anything useful but points to some other underlying issue.
					 * Don't report this error. Ensure a triage log is being create for the underlying issue
					 */
					
					return;
				}
				break; /* 3321 */
				
			case 3322: /* Device binding failure */
				description = "Device binding failure. DRM data cached by the player at /opt/persistent/adobe, may be corrupt, missing, or inaccessible due to file permission. Please check this folder. A factory reset may be required to fix this and force a re-individualization of the box to reset that data.";
				break; /* 3322 */
				
			case 3328:
				if(minor == 1003532)
				{
					description = "Potential server issue. This could happen if drm keys are missing or bad. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";
				}
				break; /* 3328 */
				
			case 3329: /* Application errors (our consec errors) */
				description = "MSO license server error response. This could happen for various reasons: bad cache data, bad session token, any license related issue. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";
				break; /* 3329 */
				
			case 3338: /* Unknown connection type */
				description = "Unknown connection type. Rare issue related to output protection code not being implemented on certain branches or core or for new socs. See STBI-6542 for details. Reach out to Receiver IP-Video team for help.";
				break; /* 3338 */
		}
		
		if(description.empty())
		{
			description = "Unrecognized error. Please report this to the STB IP-Video team.";
		}
		
		logprintf( eLOGLEVEL_ERROR, __FUNCTION__, __LINE__, "AAMPLogDRMError error=%d.%d description='%s'", major, minor, description.c_str());
	}
	
	/**
	 * @fn LogABRInfo
	 *
	 * @param[in] pstAbrInfo - pointer to a structure which will have abr info to be logged
	 * @return void
	 */
	static void LogABRInfo(AAMPAbrInfo *pstAbrInfo)
	{
		if (pstAbrInfo)
		{
			std::string reason;
			std::string profile;
			std::string symptom;
			
			if (pstAbrInfo->desiredBandwidth > pstAbrInfo->currentBandwidth)
			{
				profile = "higher";
				symptom = "video quality may increase";
			}
			else
			{
				profile = "lower";
				symptom = "video quality may decrease";
			}
			
			switch(pstAbrInfo->abrCalledFor)
			{
				case AAMPAbrBandwidthUpdate:
					reason = (pstAbrInfo->desiredBandwidth > pstAbrInfo->currentBandwidth) ? "bandwidth is good enough" : "not enough bandwidth";
					break; /* AAMPAbrBandwidthUpdate */
					
				case AAMPAbrManifestDownloadFailed:
					reason = "manifest download failed'";
					break; /* AAMPAbrManifestDownloadFailed */
					
				case AAMPAbrFragmentDownloadFailed:
					reason = "fragment download failed'";
					break; /* AAMPAbrFragmentDownloadFailed */
					
				case AAMPAbrUnifiedVideoEngine:
					reason = "changed based on unified video engine user preferred bitrate";
					break; /* AAMPAbrUserRequest */
			}
			
			if(pstAbrInfo->errorType == AAMPNetworkErrorHttp)
			{
				reason += " error='http error ";
				reason += std::to_string(pstAbrInfo->errorCode);
				symptom += " (or) freeze/buffering";
			}
			
			logprintf( eLOGLEVEL_WARN, __FUNCTION__, __LINE__, "AAMPLogABRInfo : switching to '%s' profile '%d -> %d' currentBandwidth[%ld]->desiredBandwidth[%ld] nwBandwidth[%ld] reason='%s' symptom='%s'",
					  profile.c_str(), pstAbrInfo->currentProfileIndex, pstAbrInfo->desiredProfileIndex, pstAbrInfo->currentBandwidth,
					  pstAbrInfo->desiredBandwidth, pstAbrInfo->networkBandwidth, reason.c_str(), symptom.c_str());
		}
	}
	
	/**
	 * @fn isLogLevelAllowed
	 *
	 * @param[in] chkLevel - log level
	 * @retval true if the log level allowed for print mechanism
	 */
	static bool isLogLevelAllowed(AAMP_LogLevel chkLevel)
	{
		return (chkLevel>=aampLoglevel);
	}
	
	/**
	 * @fn setLogLevel
	 *
	 * @param[in] newLevel - log level new value
	 * @return void
	 */
	static void setLogLevel(AAMP_LogLevel newLevel)
	{
		if( !locked )
		{
			aampLoglevel = newLevel;
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
	 * @fn isLogworthyErrorCode
	 * @param[in] errorCode - curl error
	 * @return true if it is not a curl error 23 and 42, because those are not real network errors.
	 */
	static bool isLogworthyErrorCode(int errorCode)
	{
		bool returnValue = false;
		
		if ((errorCode !=CURLE_OK) && (errorCode != CURLE_WRITE_ERROR) && (errorCode != CURLE_ABORTED_BY_CALLBACK))
		{
			returnValue = true;
		}
		
		return returnValue;
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

extern thread_local int gPlayerId;

class UsingPlayerId
{
private:
	int oldPlayerId;
public:
	UsingPlayerId( int playerId ): oldPlayerId(gPlayerId)
	{
		gPlayerId = playerId;
	}
	~UsingPlayerId()
	{
		gPlayerId = oldPlayerId;
	}
};

/* Context-free utility function */

/**
 * @fn DumpBlob
 *
 * @param[in] ptr to the buffer
 * @param[in] len length of buffer
 *
 * @return void
 */
void DumpBlob(const unsigned char *ptr, size_t len);

#define AAMPCLI_TIMESTAMP_PREFIX_MAX_CHARS 20
#define AAMPCLI_TIMESTAMP_PREFIX_FORMAT "%u.%03u: "

/**
 * @brief convenience macro for logging framework
 *
 * @param level gives priority for the logging, which drives filtering of whether it should be presented.  This parameter also is used as indirection to get a human readable for log level name i.e. "INFO" "WARN"
 *
 * @param FORMAT is standard printf style format string followed by arguments
 */
#define AAMPLOG( LEVEL, FORMAT, ... ) \
do { \
if( (LEVEL) >= AampLogManager::aampLoglevel ) \
{ \
logprintf( LEVEL, __FUNCTION__, __LINE__, FORMAT, ##__VA_ARGS__); \
} \
} while(0)

/**
 * @brief AAMP logging defines, this can be enabled through setLogLevel() as per the need
 */
#define AAMPLOG_TRACE(FORMAT, ...) AAMPLOG(eLOGLEVEL_TRACE, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_DEBUG(FORMAT, ...) AAMPLOG(eLOGLEVEL_DEBUG, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_INFO(FORMAT, ...)  AAMPLOG(eLOGLEVEL_INFO, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_WARN(FORMAT, ...)  AAMPLOG(eLOGLEVEL_WARN, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_MIL(FORMAT, ...)   AAMPLOG(eLOGLEVEL_MIL, FORMAT, ##__VA_ARGS__)
#define AAMPLOG_ERR(FORMAT, ...)   AAMPLOG(eLOGLEVEL_ERROR, FORMAT, ##__VA_ARGS__)

#endif /* AAMPLOGMANAGER_H */
