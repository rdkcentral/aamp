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
 * @brief AAMP logging mechanisum source file
 */

#include <iomanip>
#include <algorithm>
#include <thread>
#include <sstream>
#include "priv_aamp.h"
using namespace std;

#ifdef USE_SYSLOG_HELPER_PRINT
#include "syslog_helper_ifc.h"
#endif
#ifdef USE_SYSTEMD_JOURNAL_PRINT
#include <systemd/sd-journal.h>
#endif

// If a log line is too long, truncate it and add this long line suffix at the end
#define	LONG_LINE_SUFFIX "(...)"

/**
 * @brief Log file and cfg directory path - To support dynamic directory configuration
 */
static const char *gAampLog = "./aamp.log";
static const char *gAampCfg = "/opt/aamp.cfg";
static const char *gAampCliCfg = "/opt/aampcli.cfg";

bool gAampcliQuietLogs;

/*-----------------------------------------------------------------------------------------------------*/
bool AampLogManager::disableLogRedirection = false;

/**
 *  @brief To check the given log level is allowed to print mechanism
 */
bool AampLogManager::isLogLevelAllowed(AAMP_LogLevel chkLevel)
{
	return (chkLevel>=aampLoglevel);
}

/**
 *  @brief Set the log level for print mechanism
 */
void AampLogManager::setLogLevel(AAMP_LogLevel newLevel)
{
	if(!info && !debug && !trace)
		aampLoglevel = newLevel;
}

/**
 *  @brief Get aamp cfg directory.
 */
const char* AampLogManager::getAampCfgPath(void)
{
	return gAampCfg;
}

/**
 *  @brief Get aampcli cfg directory.
 */
const char* AampLogManager::getAampCliCfgPath(void)
{
	return gAampCliCfg;
}

/**
 *  @brief Get a hex string representation of a vector of bytes
 */
std::string AampLogManager::getHexDebugStr(const std::vector<uint8_t>& data)
{
	std::ostringstream hexSs;
	hexSs << "0x";
	hexSs << std::hex << std::uppercase << std::setfill('0');
	std::for_each(data.cbegin(), data.cend(), [&](int c) { hexSs << std::setw(2) << c; });
	return hexSs.str();
}

/**
 *  @brief Print the network latency level logging for triage purpose
 */
void AampLogManager::LogNetworkLatency(const char* url, int downloadTime, int downloadThresholdTimeoutMs, AampMediaType type)
{
	std::string location;
	std::string symptom;

	ParseContentUrl(url, location, symptom, type);

	AAMPLOG(this, eLOGLEVEL_WARN, "AAMPLogNetworkLatency downloadTime=%d downloadThreshold=%d type='%s' location='%s' symptom='%s' url='%s'",
		downloadTime, downloadThresholdTimeoutMs, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url);
}

/**
 *  @brief Print the network error level logging for triage purpose
 */
void AampLogManager::LogNetworkError(const char* url, AAMPNetworkErrorType errorType, int errorCode, AampMediaType type)
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
				AAMPLOG(this, eLOGLEVEL_ERROR, "AAMPLogNetworkError error='http error %d' type='%s' location='%s' symptom='%s' url='%s'",
					errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
			}
		}
			break; /*AAMPNetworkErrorHttp*/

		case AAMPNetworkErrorTimeout:
		{
			if(errorCode > 0)
			{
				AAMPLOG(this, eLOGLEVEL_ERROR, "AAMPLogNetworkError error='timeout %d' type='%s' location='%s' symptom='%s' url='%s'",
					errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
			}
		}
			break; /*AAMPNetworkErrorTimeout*/

		case AAMPNetworkErrorCurl:
		{
			if(errorCode > 0)
			{
				AAMPLOG(this, eLOGLEVEL_ERROR, "AAMPLogNetworkError error='curl error %d' type='%s' location='%s' symptom='%s' url='%s'",
					errorCode, GetMediaTypeName(type), location.c_str(), symptom.c_str(), url );
			}
		}
			break; /*AAMPNetworkErrorCurl*/

		case AAMPNetworkErrorNone:
			break;
	}
}


/**
 *  @brief To get the issue symptom based on the error type for triage purpose
 */
void AampLogManager::ParseContentUrl(const char* url, std::string& location, std::string& symptom, AampMediaType type)
{
	switch (type)
	{
		case eMEDIATYPE_MANIFEST:
		{
			symptom = "video fails to start, has delayed start or freezes/buffers";
		}
			break;

		case eMEDIATYPE_PLAYLIST_VIDEO:
		case eMEDIATYPE_PLAYLIST_AUDIO:
		case eMEDIATYPE_PLAYLIST_IFRAME:
		{
			symptom = "video fails to start or freeze/buffering";
		}
			break;

		case eMEDIATYPE_INIT_VIDEO:
		case eMEDIATYPE_INIT_AUDIO:
		case eMEDIATYPE_INIT_IFRAME:
		{
			symptom = "video fails to start";
		}
			break;

		case eMEDIATYPE_VIDEO:
		{
			symptom = "freeze/buffering";
		}
			break;

		case eMEDIATYPE_AUDIO:
		{
			symptom = "audio drop or freeze/buffering";
		}
			break;

		case eMEDIATYPE_IFRAME:
		{
			symptom = "trickplay ends or freezes";
		}
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
 *  @brief Print the DRM error level logging for triage purpose
 */
void AampLogManager::LogDRMError(int major, int minor)
{
	std::string description;

	switch(major)
	{
		case 3307: /* Internal errors */
		{
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
				description = "Local cache directory not readable. The Receiver running as non-root cannot access and read the adobe cache at /opt/persistent/adobe. This can happen if /lib/rdk/prepareChrootEnv.sh fails to set that folders privileges. Run ls -l /opt/persistent and check the access rights. Contact the SI team for help. Also see jira XRE-6687";
			}
		}
			break; /* 3307 */

		case 3321: /* Individualization errors */
		{
			if(minor == 102)
			{
				description = "Invalid signiture request on the Adobe individualization request. Expired certs can cause this, so the first course of action is to verify if the certs, temp baked in or production fkps, have not expired.";
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
		}
			break; /* 3321 */

		case 3322: /* Device binding failure */
		{
			description = "Device binding failure. DRM data cached by the player at /opt/persistent/adobe, may be corrupt, missing, or innaccesible due to file permision. Please check this folder. A factory reset may be required to fix this and force a re-individualization of the box to reset that data.";
		}
			break; /* 3322 */

		case 3328:
		{
			if(minor == 1003532)
			{
				description = "Potential server issue. This could happen if drm keys are missing or bad. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";
			}
		}
			break; /* 3328 */

		case 3329: /* Application errors (our consec errors) */
		{
			description = "MSO license server error response. This could happen for various reasons: bad cache data, bad session token, any license related issue. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";
		}
			break; /* 3329 */

		case 3338: /* Unknown connection type */
		{
			description = "Unknown connection type. Rare issue related to output protection code not being implemented on certain branches or core or for new socs. See STBI-6542 for details. Reach out to Receiver IP-Video team for help.";
		}
			break; /* 3338 */
	}

	if(description.empty())
	{
		description = "Unrecognized error. Please report this to the STB IP-Video team.";
	}

	AAMPLOG(this, eLOGLEVEL_ERROR, "AAMPLogDRMError error=%d.%d description='%s'", major, minor, description.c_str());
}

/**
 *  @brief Log ABR info for triage purpose
 */
void AampLogManager::LogABRInfo(AAMPAbrInfo *pstAbrInfo)
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
			{
				reason = (pstAbrInfo->desiredBandwidth > pstAbrInfo->currentBandwidth) ? "bandwidth is good enough" : "not enough bandwidth";
			}
				break; /* AAMPAbrBandwidthUpdate */

			case AAMPAbrManifestDownloadFailed:
			{
				reason = "manifest download failed'";
			}
				break; /* AAMPAbrManifestDownloadFailed */

			case AAMPAbrFragmentDownloadFailed:
			{
				reason = "fragment download failed'";
			}
				break; /* AAMPAbrFragmentDownloadFailed */

			case AAMPAbrUnifiedVideoEngine:
			{
				reason = "changed based on unified video engine user preferred bitrate";
			}
				break; /* AAMPAbrUserRequest */
		}

		if(pstAbrInfo->errorType == AAMPNetworkErrorHttp)
		{
			reason += " error='http error ";
			reason += to_string(pstAbrInfo->errorCode);
			symptom += " (or) freeze/buffering";
		}

		AAMPLOG(this, eLOGLEVEL_WARN, "AAMPLogABRInfo : switching to '%s' profile '%d -> %d' currentBandwidth[%ld]->desiredBandwidth[%ld] nwBandwidth[%ld] reason='%s' symptom='%s'",
			profile.c_str(), pstAbrInfo->currentProfileIndex, pstAbrInfo->desiredProfileIndex, pstAbrInfo->currentBandwidth,
			pstAbrInfo->desiredBandwidth, pstAbrInfo->networkBandwidth, reason.c_str(), symptom.c_str());
	}
}

/**
 *  @brief Check curl error before log on console
 */
bool AampLogManager::isLogworthyErrorCode(int errorCode)
{
	bool returnValue = false;

	if ((errorCode !=0) && (errorCode != CURLE_WRITE_ERROR) && (errorCode != CURLE_ABORTED_BY_CALLBACK))
	{
		returnValue = true;
	}

	return returnValue;
}

static FILE *OpenSimulatorLogFile( void )
{
	static bool init;
	FILE *f = fopen(gAampLog, (init ? "a" : "w"));
	init = true;
	return f;
}

/**
 * @brief Print one log line
 */
void logprintline(FILE *f, struct timeval t, const char* printBuffer)
{
	(void)fprintf(f, "%ld.%03ld: %s\n", (long int)t.tv_sec, (long int)t.tv_usec / 1000, printBuffer);
}

static const char *mLogLevelStr[eLOGLEVEL_ERROR+1] =
{
	"TRACE", // eLOGLEVEL_TRACE
	"DEBUG", // eLOGLEVEL_DEBUG
	"INFO",  // eLOGLEVEL_INFO
	"WARN",  // eLOGLEVEL_WARN
	"MIL",   // eLOGLEVEL_MIL
	"ERROR", // eLOGLEVEL_ERROR
};

/**
 * @brief Print logs to console / log file
 */
void logprintf(int playerId, AAMP_LogLevel logLevelIndex, const char* file, int line, const char *format, ...)
{
	// logLevelIndex is enum, so lookup in mLogLevelStr should always be safe
	assert( logLevelIndex<ARRAY_SIZE(mLogLevelStr) );
	va_list args;
	va_start(args, format);
	char gDebugPrintBuffer[MAX_DEBUG_LOG_BUFF_SIZE];
	std::ostringstream ossthread;
	ossthread << std::this_thread::get_id();
	int len_header = snprintf(gDebugPrintBuffer, sizeof(gDebugPrintBuffer),
							  "[AAMP-PLAYER][%d][%s][%s][%s][%d]",
							  playerId,
							  mLogLevelStr[logLevelIndex],
							  ossthread.str().c_str(),
							  file,
							  line);
	int len_message = 0;
	if (len_header >= sizeof(gDebugPrintBuffer))
	{ // Header is too long to print in one log line, no space left for the message
	}
	else
	{
		if (len_header < 0)
		{ // Encoding error, let's print only the message
			len_header = 0;
		}

		len_message = vsnprintf(gDebugPrintBuffer+len_header, MAX_DEBUG_LOG_BUFF_SIZE-len_header, format, args);
		if (len_message < 0)
		{ // Encoding error, let's print only the header
			len_message = 0;
		}
	}
	int len_total = len_header + len_message;
	if (len_total >= sizeof(gDebugPrintBuffer))
	{
		// If the log line is too long, truncate it and add the long line suffix at the end
		(void)snprintf(gDebugPrintBuffer + MAX_DEBUG_LOG_BUFF_SIZE - sizeof(LONG_LINE_SUFFIX), sizeof(LONG_LINE_SUFFIX), LONG_LINE_SUFFIX);
	}
	//gDebugPrintBuffer[(MAX_DEBUG_LOG_BUFF_SIZE-1)] = 0;
	va_end(args);

#if (defined (USE_SYSTEMD_JOURNAL_PRINT) || defined (USE_SYSLOG_HELPER_PRINT))
	if(!AampLogManager::disableLogRedirection)
	{
#ifdef USE_SYSTEMD_JOURNAL_PRINT
		sd_journal_print(LOG_NOTICE, "%s", gDebugPrintBuffer);
#else
		send_logs_to_syslog(gDebugPrintBuffer);
#endif
		return;
	}
#endif

	struct timeval t;
	gettimeofday(&t, NULL);
    
    if( !gAampcliQuietLogs )
    {
        logprintline(stdout, t, gDebugPrintBuffer);
    }

#ifdef AAMP_SIMULATOR_BUILD
	// When running the simulator also print the log to a file
	FILE *f = OpenSimulatorLogFile();
	if (f)
	{
		logprintline(f, t, gDebugPrintBuffer);
		(void)fclose(f);
	}
#endif
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
