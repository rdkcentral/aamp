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
 * @file jsutils.cpp
 * @brief JavaScript util functions for AAMP
 */


#include "jsutils.h"

#include <stdlib.h>
#include <stdio.h>
#include <cmath>
#include <stdarg.h>
#include <iomanip>
#include <algorithm>
#ifdef USE_SYSLOG_HELPER_PRINT
#include "syslog_helper_ifc.h"
#endif
#ifdef USE_SYSTEMD_JOURNAL_PRINT
#include <systemd/sd-journal.h>
#endif

#ifdef USE_ETHAN_LOG
#include <ethanlog.h>
#else
// stubs for use if USE_ETHAN_LOG not defined
void ethanlog(int level, const char *filename, const char *function, int line, const char *format, ...){}
#define ETHAN_LOG_INFO 0
#define ETHAN_LOG_DEBUG 1
#define ETHAN_LOG_WARNING 2
#define ETHAN_LOG_ERROR 3
#define ETHAN_LOG_FATAL 4
#define ETHAN_LOG_MILESTONE 5
#endif

#define MAX_DEBUG_LOG_BUFF_SIZE 1024

VideoZoomMode MapZoomMode( const char *zoomStr )
{
	VideoZoomMode zoom = VIDEO_ZOOM_FULL; // default
	if( zoomStr )
	{
		static const char *name[]
		{
			"none", // VIDEO_ZOOM_NONE
			"direct", // VIDEO_ZOOM_DIRECT
			"normal", // VIDEO_ZOOM_NORMAL
			"stretch", // VIDEO_ZOOM_16X9_STRETCH
			"pillar", // VIDEO_ZOOM_4x3_PILLAR_BOX
			"full", // VIDEO_ZOOM_FULL
			"global", // VIDEO_ZOOM_GLOBAL
		};
		for( int i=0; i<ARRAY_SIZE(name); i++ )
		{
			if (0 == strcmp(zoomStr, name[i]) )
			{
				zoom = (VideoZoomMode)i;
			}
		}
	}
	return zoom;
}

/**
 * @struct EventTypeMap
 * @brief Struct to map names of AAMP events and JS events
 */
struct EventTypeMap
{
	AAMPEventType eventType;
	const char* szName;
};


/**
 * @brief Map AAMP events to its corresponding JS event strings (used by JSPP)
 */
static EventTypeMap aamp_eventTypes[] =
{
	{ (AAMPEventType)0, "onEvent"},
	{ AAMP_EVENT_TUNED, "tuned"},
	{ AAMP_EVENT_TUNE_FAILED, "tuneFailed"},
	{ AAMP_EVENT_SPEED_CHANGED, "speedChanged"},
	{ AAMP_EVENT_EOS, "eos"},
	{ AAMP_EVENT_PLAYLIST_INDEXED, "playlistIndexed"},
	{ AAMP_EVENT_PROGRESS, "progress"},
	{ AAMP_EVENT_CC_HANDLE_RECEIVED, "decoderAvailable"},
	{ AAMP_EVENT_JS_EVENT, "jsEvent"},
	{ AAMP_EVENT_MEDIA_METADATA, "metadata"},
	{ AAMP_EVENT_ENTERING_LIVE, "enteringLive"},
	{ AAMP_EVENT_BITRATE_CHANGED, "bitrateChanged"},
	{ AAMP_EVENT_TIMED_METADATA, "timedMetadata"},
	{ AAMP_EVENT_BULK_TIMED_METADATA, "bulkTimedMetadata"},
	{ AAMP_EVENT_STATE_CHANGED, "statusChanged"},
	{ AAMP_EVENT_SPEEDS_CHANGED, "speedsChanged"},
	{ AAMP_EVENT_SEEKED, "seeked"},
	{ AAMP_EVENT_DRM_METADATA, "drmMetadata"},
	{ AAMP_EVENT_REPORT_ANOMALY, "anomalyReport" },
	{ AAMP_EVENT_AD_RESOLVED, "adResolved"},
	{ AAMP_EVENT_AD_RESERVATION_START, "reservationStart" },
	{ AAMP_EVENT_AD_RESERVATION_END, "reservationEnd" },
	{ AAMP_EVENT_AD_PLACEMENT_START, "placementStart" },
	{ AAMP_EVENT_AD_PLACEMENT_END, "placementEnd" },
	{ AAMP_EVENT_AD_PLACEMENT_PROGRESS, "placementProgress" },
	{ AAMP_EVENT_AD_PLACEMENT_ERROR, "placementError" },
	{ AAMP_EVENT_REPORT_METRICS_DATA, "metricsData" },
	{ AAMP_EVENT_BUFFERING_CHANGED, "bufferingChanged"},
	{ AAMP_EVENT_ID3_METADATA, "id3Metadata"},
	{ AAMP_EVENT_DRM_MESSAGE, "drmMessage" },
	{ AAMP_EVENT_AUDIO_TRACKS_CHANGED, "audioTracksChanged"},
	{ AAMP_EVENT_TEXT_TRACKS_CHANGED, "textTracksChanged"},
	{ AAMP_EVENT_CONTENT_GAP, "contentGap" },
	{ AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE, "contentProtectionDataUpdate" },
	{ AAMP_EVENT_MANIFEST_REFRESH_NOTIFY, "manifestRefresh"},
	{ AAMP_EVENT_TUNE_TIME_METRICS, "tuneMetricsData" },
	{ AAMP_EVENT_NEED_MANIFEST_DATA, "needManifest" },
	{ AAMP_EVENT_MONITORAV_STATUS, "monitorAVStatus"},
	{ (AAMPEventType)0, "" }
};


/**
 * @brief Map AAMP events to its corresponding JS event strings (used by AAMPMediaPlayer/UVE APIs)
 */
static EventTypeMap aampPlayer_eventTypes[] =
{
//TODO: Need separate event list to avoid breaking existing legacy impl. Unify later.
	{ (AAMPEventType)0, "onEvent"},
	{ AAMP_EVENT_TUNED, "playbackStarted"},
	{ AAMP_EVENT_TUNE_FAILED, "playbackFailed"},
	{ AAMP_EVENT_SPEED_CHANGED, "playbackSpeedChanged"},
	{ AAMP_EVENT_EOS, "playbackCompleted"},
	{ AAMP_EVENT_PLAYLIST_INDEXED, "playlistIndexed"},
	{ AAMP_EVENT_PROGRESS, "playbackProgressUpdate"},
	{ AAMP_EVENT_CC_HANDLE_RECEIVED, "decoderAvailable"},
	{ AAMP_EVENT_JS_EVENT, "jsEvent"},
	{ AAMP_EVENT_MEDIA_METADATA, "mediaMetadata"},
	{ AAMP_EVENT_ENTERING_LIVE, "enteringLive"},
	{ AAMP_EVENT_BITRATE_CHANGED, "bitrateChanged"},
	{ AAMP_EVENT_TIMED_METADATA, "timedMetadata"},
	{ AAMP_EVENT_BULK_TIMED_METADATA, "bulkTimedMetadata"},
	{ AAMP_EVENT_STATE_CHANGED, "playbackStateChanged"},
	{ AAMP_EVENT_SPEEDS_CHANGED, "speedsChanged"},
	{ AAMP_EVENT_SEEKED, "seeked"},
	{ AAMP_EVENT_TUNE_PROFILING, "tuneProfiling"},
	{ AAMP_EVENT_BUFFERING_CHANGED, "bufferingChanged"},
	{ AAMP_EVENT_DURATION_CHANGED, "durationChanged"},
	{ AAMP_EVENT_AUDIO_TRACKS_CHANGED, "audioTracksChanged"},
	{ AAMP_EVENT_TEXT_TRACKS_CHANGED, "textTracksChanged"},
	{ AAMP_EVENT_AD_BREAKS_CHANGED, "contentBreaksChanged"},
	{ AAMP_EVENT_AD_STARTED, "contentStarted"},
	{ AAMP_EVENT_AD_COMPLETED, "contentCompleted"},
	{ AAMP_EVENT_DRM_METADATA, "drmMetadata"},
	{ AAMP_EVENT_REPORT_ANOMALY, "anomalyReport" },
	{ AAMP_EVENT_WEBVTT_CUE_DATA, "vttCueDataListener" },
	{ AAMP_EVENT_AD_RESOLVED, "adResolved"},
	{ AAMP_EVENT_AD_RESERVATION_START, "reservationStart" },
	{ AAMP_EVENT_AD_RESERVATION_END, "reservationEnd" },
	{ AAMP_EVENT_AD_PLACEMENT_START, "placementStart" },
	{ AAMP_EVENT_AD_PLACEMENT_END, "placementEnd" },
	{ AAMP_EVENT_AD_PLACEMENT_ERROR, "placementError" },
	{ AAMP_EVENT_AD_PLACEMENT_PROGRESS, "placementProgress" },
	{ AAMP_EVENT_REPORT_METRICS_DATA, "metricsData" },
	{ AAMP_EVENT_ID3_METADATA, "id3Metadata"},
	{ AAMP_EVENT_DRM_MESSAGE, "drmMessage" },
	{ AAMP_EVENT_BLOCKED, "blocked" },
	{ AAMP_EVENT_CONTENT_GAP, "contentGap" },
	{ AAMP_EVENT_HTTP_RESPONSE_HEADER, "httpResponseHeader"},
	{ AAMP_EVENT_WATERMARK_SESSION_UPDATE, "watermarkSessionUpdate" },
	{ AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE, "contentProtectionDataUpdate" },
	{ AAMP_EVENT_MANIFEST_REFRESH_NOTIFY, "manifestRefresh"},
	{ AAMP_EVENT_TUNE_TIME_METRICS, "tuneMetricsData" },
	{ AAMP_EVENT_NEED_MANIFEST_DATA, "needManifest" },
	{ AAMP_EVENT_MONITORAV_STATUS, "monitorAVStatus" },
	{ (AAMPEventType)0, "" }
};

/**
 * @brief Convert C string to JSString
 */
JSValueRef aamp_CStringToJSValue(JSContextRef context, const char* sz)
{
	JSStringRef str = JSStringCreateWithUTF8CString(sz);
	JSValueRef value = JSValueMakeString(context, str);
	JSStringRelease(str);

	return value;
}

/**
 * @brief Convert JSString to C string
 */
char* aamp_JSValueToCString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
	JSStringRef jsstr = JSValueToStringCopy(context, value, exception);
	size_t len = JSStringGetMaximumUTF8CStringSize(jsstr);
	char* src = new char[len];
	JSStringGetUTF8CString(jsstr, src, len);
	JSStringRelease(jsstr);
	return src;
}

/**
 * @brief Convert JSString to JSON C string
 */
char* aamp_JSValueToJSONCString(JSContextRef context, JSValueRef value, JSValueRef* exception)
{
        JSStringRef jsstr = JSValueCreateJSONString(context, value, 0, exception);
        size_t len = JSStringGetMaximumUTF8CStringSize(jsstr);
        char* src = new char[len];
        JSStringGetUTF8CString(jsstr, src, len);
        JSStringRelease(jsstr);
        return src;
}

/**
 * @brief Check if a JSValue object is array or not
 */
bool aamp_JSValueIsArray(JSContextRef context, JSValueRef value)
{
	JSObjectRef global = JSContextGetGlobalObject(context);
	JSStringRef arrayProp = JSStringCreateWithUTF8CString("Array");
	JSValueRef arrayVal = JSObjectGetProperty(context, global, arrayProp, NULL);
	JSStringRelease(arrayProp);

	if (JSValueIsObject(context, arrayVal))
	{
		JSObjectRef arrayObj = JSValueToObject(context, arrayVal, NULL);
		if (JSObjectIsFunction(context, arrayObj) || JSObjectIsConstructor(context, arrayObj))
		{
			return JSValueIsInstanceOfConstructor(context, value, arrayObj, NULL);
		}
	}

	return false;
}


/**
 * @brief Convert an array of JSString to an array of C strings
 */
std::vector<std::string> aamp_StringArrayToCStringArray(JSContextRef context, JSValueRef arrayRef)
{
    std::vector<std::string> retval;
    JSValueRef exception = NULL;

    if(!arrayRef)
    {
        LOG_ERROR_EX("Error: value is NULL.");
        return retval;
    }
    if (!JSValueIsObject(context, arrayRef))
    {
        LOG_ERROR_EX("Error: value is not an object.");
        return retval;
    }
    if(!aamp_JSValueIsArray(context, arrayRef))
    {
        LOG_ERROR_EX("Error: value is not an array.");
        return retval;
    }
    JSObjectRef arrayObj = JSValueToObject(context, arrayRef, &exception);
    if(exception)
    {

        LOG_ERROR_EX("Error: exception accessing array object.");
        return retval;
    }

    JSStringRef lengthStrRef = JSStringCreateWithUTF8CString("length");
    JSValueRef lengthRef = JSObjectGetProperty(context, arrayObj, lengthStrRef, &exception);
    if(exception)
    {

        LOG_ERROR_EX("Error: exception accessing array length.");
        return retval;
    }
    int length = JSValueToNumber(context, lengthRef, &exception);
    if(exception)
    {
        LOG_ERROR_EX("Error: exception array length in not a number.");
        return retval;
    }

    retval.reserve(length);
    for(int i = 0; i < length; i++)
    {
        JSValueRef strRef = JSObjectGetPropertyAtIndex(context, arrayObj, i, &exception);
        if(exception)
            continue;

        char* str = aamp_JSValueToCString(context, strRef, NULL);
        LOG_TRACE("array[%d] = '%s'.",i,str);
        retval.push_back(str);
        SAFE_DELETE_ARRAY(str);
    }

    JSStringRelease(lengthStrRef);

    return retval;
}


/**
 * @brief Generate a JSValue object with the exception details
 */
JSValueRef aamp_GetException(JSContextRef context, ErrorCode error, const char *additionalInfo)
{
	const char *str;
	JSValueRef retVal;

	switch(error)
	{
		case AAMPJS_INVALID_ARGUMENT:
		case AAMPJS_MISSING_OBJECT:
			str = "TypeError";
			break;
		default:
			str = "Generic Error";
			break;
	}

	char exceptionMsg[EXCEPTION_ERR_MSG_MAX_LEN];
	memset(exceptionMsg, '\0', EXCEPTION_ERR_MSG_MAX_LEN);

	if(additionalInfo)
	{
		snprintf(exceptionMsg, EXCEPTION_ERR_MSG_MAX_LEN - 1, "%s: %s", str, additionalInfo);
	}
	else
	{
		snprintf(exceptionMsg, EXCEPTION_ERR_MSG_MAX_LEN - 1, "%s!!", str);
	}


        LOG_WARN_EX("exception=%s",exceptionMsg);
        
	const JSValueRef arguments[] = { aamp_CStringToJSValue(context, exceptionMsg) };
	JSValueRef exception = NULL;
	retVal = JSObjectMakeError(context, 1, arguments, &exception);
	if (exception)
	{
                LOG_ERROR_EX("Error: exception creating an error object");
		return NULL;
	}

	return retVal;
}

/**
 * @brief Convert JS event name to AAMP event type
 */
AAMPEventType aamp_getEventTypeFromName(const char* szName)
{
	AAMPEventType eventType = AAMP_MAX_NUM_EVENTS;
	int numEvents = sizeof(aamp_eventTypes) / sizeof(aamp_eventTypes[0]);

	for (int i=0; i<numEvents; i++)
	{
		if (strcasecmp(aamp_eventTypes[i].szName, szName) == 0)
		{
			eventType = aamp_eventTypes[i].eventType;
			break;
		}
	}

	return eventType;
}

/**
 * @brief To dispatch a JS event
 */
void aamp_dispatchEventToJS(JSContextRef context, JSObjectRef callback, JSObjectRef event)
{
	JSValueRef args[1] = { event };
	if (context != NULL && callback != NULL)
	{
		JSObjectCallAsFunction(context, callback, NULL, 1, args, NULL);
	}
}

/**
 * @brief Convert JS event name to AAMP event type (AAMPMediaPlayer)
 */
AAMPEventType aampPlayer_getEventTypeFromName(const char* szName)
{
//TODO: Need separate event list for now to avoid breaking existing viper impl. Unify later
	AAMPEventType eventType = AAMP_MAX_NUM_EVENTS;
	int numEvents = sizeof(aampPlayer_eventTypes) / sizeof(aampPlayer_eventTypes[0]);

	for (int i=0; i<numEvents; i++)
	{
		if (strcasecmp(aampPlayer_eventTypes[i].szName, szName) == 0)
		{
			eventType = aampPlayer_eventTypes[i].eventType;
			break;
		}
	}

	return eventType;
}

/**
 * @brief Convert AAMP event type to JS event string (AAMPMediaPlayer)
 */
const char* aampPlayer_getNameFromEventType(AAMPEventType type)
{
//TODO: Need separate API to avoid breaking existing viper impl. Unify later.
	if (type > 0 && type < AAMP_MAX_NUM_EVENTS)
	{
		return aampPlayer_eventTypes[type].szName;
	}
	else
	{
		return NULL;
	}
}


/**
 * @brief Create a TimedMetadata JS object with args passed.
 *        Sample input "#EXT-X-CUE:ID=eae90713-db8e,DURATION=30.063"
 *        Sample output {"time":62062,"duration":0,"name":"#EXT-X-CUE","content":"-X-CUE:ID=eae90713-db8e,DURATION=30.063","type":0,"metadata":{"ID":"eae90713-db8e","DURATION":"30.063"},"id":"eae90713-db8e"}
 */
JSObjectRef aamp_CreateTimedMetadataJSObject(JSContextRef context, long long timeMS, const char* szName, const char* szContent, const char* id, double durationMS)
{
	JSStringRef name;

	JSObjectRef timedMetadata = JSObjectMake(context, NULL, NULL);

	if (timedMetadata) {
		JSValueProtect(context, timedMetadata);
		bool bGenerateID = true;

		name = JSStringCreateWithUTF8CString("time");
		JSObjectSetProperty(context, timedMetadata, name, JSValueMakeNumber(context, std::round(timeMS)), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		// For SCTE35 tag, set id as value of key reservationId
		if(!strcmp(szName, "SCTE35") && id && *id != '\0')
		{
			name = JSStringCreateWithUTF8CString("reservationId");
			JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, id), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
			bGenerateID = false;
		}

		if (durationMS >= 0)
		{
			name = JSStringCreateWithUTF8CString("duration");
			JSObjectSetProperty(context, timedMetadata, name, JSValueMakeNumber(context, (int)durationMS), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}

		name = JSStringCreateWithUTF8CString("name");
		JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, szName), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		name = JSStringCreateWithUTF8CString("content");
		JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, szContent), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		// Force type=0 (HLS tag) for now.
		// Does type=1 ID3 need to be supported?
		name = JSStringCreateWithUTF8CString("type");
		JSObjectSetProperty(context, timedMetadata, name, JSValueMakeNumber(context, 0), kJSPropertyAttributeReadOnly, NULL);
		JSStringRelease(name);

		// Force metadata as empty object
		JSObjectRef metadata = JSObjectMake(context, NULL, NULL);
		if (metadata) {
			JSValueProtect(context, metadata);
			name = JSStringCreateWithUTF8CString("metadata");
			JSObjectSetProperty(context, timedMetadata, name, metadata, kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);

			// Parse CUE metadata and TRICKMODE-RESTRICTION metadata
			// Parsed values are used in PlayerPlatform at the time of tag object creation
			if ((strcmp(szName, "#EXT-X-CUE") == 0) ||
			    (strcmp(szName, "#EXT-X-TRICKMODE-RESTRICTION") == 0) ||
			    (strcmp(szName, "#EXT-X-MARKER") == 0) ||
			    (strcmp(szName, "#EXT-X-SCTE35") == 0)) {
				const char* szStart = szContent;

				// Parse comma separated name=value list.
				while (*szStart != '\0') {
					char* szSep;
					// Find the '=' seperator.
					for (szSep = (char*)szStart; *szSep != '=' && *szSep != '\0'; szSep++);

					// Find the end of the value.
					char* szEnd = (*szSep != '\0') ? szSep + 1 : szSep;
					for (; *szEnd != ',' && *szEnd != '\0'; szEnd++);

					// Append the name / value metadata.
					if ((szStart < szSep) && (szSep < szEnd)) {
						JSValueRef value;
						char chSave = *szSep;

						*szSep = '\0';
						name = JSStringCreateWithUTF8CString(szStart);
						*szSep = chSave;

						chSave = *szEnd;
						*szEnd = '\0';
						value = aamp_CStringToJSValue(context, szSep+1);
						*szEnd = chSave;

						JSObjectSetProperty(context, metadata, name, value, kJSPropertyAttributeReadOnly, NULL);
						JSStringRelease(name);

						// If we just added the 'ID', copy into timedMetadata.id
						if (szStart[0] == 'I' && szStart[1] == 'D' && szStart[2] == '=') {
							bGenerateID = false;
							name = JSStringCreateWithUTF8CString("id");
							JSObjectSetProperty(context, timedMetadata, name, value, kJSPropertyAttributeReadOnly, NULL);
							JSStringRelease(name);
						}
					}

					szStart = (*szEnd != '\0') ? szEnd + 1 : szEnd;
				}
			}
			// Parse TARGETDURATION and CONTENT-IDENTIFIER metadata
			else {
				const char* szStart = szContent;
				// Advance to the tag's value.
				for (; *szStart != ':' && *szStart != '\0'; szStart++);
				if (*szStart == ':')
					szStart++;

				// Stuff all content into DATA name/value pair.
				JSValueRef value = aamp_CStringToJSValue(context, szStart);
				if (strcmp(szName, "#EXT-X-TARGETDURATION") == 0) {
					// Stuff into DURATION if EXT-X-TARGETDURATION content.
					// Since #EXT-X-TARGETDURATION has only duration as value
					name = JSStringCreateWithUTF8CString("DURATION");
				} else {
					name = JSStringCreateWithUTF8CString("DATA");
				}
				JSObjectSetProperty(context, metadata, name, value, kJSPropertyAttributeReadOnly, NULL);
				JSStringRelease(name);
			}
			JSValueUnprotect(context, metadata);
		}

		// Generate an ID since the tag is missing one
		if (bGenerateID) {
			int hash = (int)timeMS;
			const char* szStart = szName;
			for (; *szStart != '\0'; szStart++) {
				hash = (hash * 33) ^ *szStart;
			}

			char buf[32];
			snprintf(buf, sizeof(buf), "%d", hash);
			name = JSStringCreateWithUTF8CString("id");
			JSObjectSetProperty(context, timedMetadata, name, aamp_CStringToJSValue(context, buf), kJSPropertyAttributeReadOnly, NULL);
			JSStringRelease(name);
		}
		JSValueUnprotect(context, timedMetadata);
	}

        return timedMetadata;
}

/**
 * @brief Create a body response JS object with args passed.
 *        Sample input "{\"errorCode\":\"OVP_00030\",\"description\":\"Invalid parameter value: bt\"}"
 *        Sample output {errorCode:"OVP_00030",description:"Invalid parameter value: bt"}
 */
JSObjectRef aamp_CreateBodyResponseJSObject(JSContextRef context, const char *pBodyResponse)
{
	JSObjectRef bodyResponseObj = JSObjectMake(context, NULL, NULL);
	JSStringRef name;

	if ( bodyResponseObj && pBodyResponse )
	{
		const char *pStringTakeOut = "\"";
		const size_t strTakeOutLen = strlen(pStringTakeOut);
		const char *pReplaceString="";
		const size_t replaceLen = strlen(pReplaceString);
		std::string bodyResponse(pBodyResponse);
		if ( bodyResponse.length()<=2 )
		{
			LOG_WARN_EX( "Invalid body response %s, aamp_CreateBodyResponseJSObject returning", bodyResponse.c_str());
			return bodyResponseObj;
		}
		bodyResponse.assign(bodyResponse.begin()+1,bodyResponse.end()-1);
		size_t pos = bodyResponse.find(","); //find main delimiter

		while ( pos != std::string::npos || false == bodyResponse.empty() )
		{
			std::string strBeforeSubDelimiter, strAfterSubDelimiter;
			size_t subPos = bodyResponse.find("\":"); //find sub delimiter within main delimiter
			if( subPos != std::string::npos )
			{
				//split the string based on sub delimiter
				strBeforeSubDelimiter.assign(bodyResponse.begin(),bodyResponse.begin()+subPos+1);
				strAfterSubDelimiter.assign(bodyResponse.begin()+subPos+2,pos == std::string::npos?bodyResponse.end():bodyResponse.begin()+pos);
				if( !strBeforeSubDelimiter.empty() )
				{
					//replace the string with pReplaceBefore string;
					size_t posBefore = strBeforeSubDelimiter.find(pStringTakeOut);
					while (posBefore != std::string::npos)
					{
						strBeforeSubDelimiter.replace(posBefore, strTakeOutLen, pReplaceString);
						posBefore = strBeforeSubDelimiter.find(pStringTakeOut, posBefore + replaceLen);
					}
					name = JSStringCreateWithUTF8CString(strBeforeSubDelimiter.c_str()); //JSString as name
				}

				if( !strAfterSubDelimiter.empty() )
				{
					//eliminate the "" string after the delimiter
					size_t posAfter = strAfterSubDelimiter.find(pStringTakeOut);
					while (posAfter != std::string::npos)
					{
						strAfterSubDelimiter.replace(posAfter, strTakeOutLen, pReplaceString);
						posAfter = strAfterSubDelimiter.find(pStringTakeOut, posAfter + replaceLen);
					}
					JSValueRef value;
					value = aamp_CStringToJSValue(context, strAfterSubDelimiter.c_str());
					JSValueProtect(context, bodyResponseObj);
					JSObjectSetProperty(context, bodyResponseObj, name, value, kJSPropertyAttributeReadOnly, NULL);
					JSStringRelease(name);
					JSValueUnprotect(context, bodyResponseObj);
				}
			}
			//find next main delimiter string
			if(pos != std::string::npos )
			{
				bodyResponse.assign(bodyResponse.begin()+pos+1,bodyResponse.end());
				pos = bodyResponse.find(",");
			}
			else
			{
				bodyResponse.clear();
			}
		}
	}
	return bodyResponseObj;
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
 * TODO: deprecate jsBindingLogprintf, and leverage common logprintf directly
 */
void jsBindingLogprintf(int playerId ,const char* functionName, int line, int logLevel, const char *format, ...)
{
	int len = 0;
	va_list args;
	va_start(args, format);

	char gDebugPrintBuffer[MAX_DEBUG_LOG_BUFF_SIZE];
	const char* levelstr = mLogLevelStr[logLevel];
	if(playerId != PLAYER_ID_NA )
	{
		len = snprintf(gDebugPrintBuffer,sizeof(gDebugPrintBuffer),"[AAMP-JS] %d :%s : %s : %d :",playerId,levelstr,functionName,line);
	}
	else
	{
		len = snprintf(gDebugPrintBuffer,sizeof(gDebugPrintBuffer),"[AAMP-JS] %s : %s: %d :",levelstr,functionName,line);
	}
	vsnprintf(gDebugPrintBuffer+len, MAX_DEBUG_LOG_BUFF_SIZE-len, format, args);
	gDebugPrintBuffer[(MAX_DEBUG_LOG_BUFF_SIZE-1)] = 0;

	va_end(args);

	if ( AampLogManager::enableEthanLogRedirection  )
	{ // ethanlog
		int ethanLogLevel;
		// Important: in production builds, Ethan logger filters out everything
		// except ETHAN_LOG_MILESTONE and ETHAN_LOG_FATAL
		switch (logLevel)
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
		ethanlog(ethanLogLevel,NULL,NULL,-1,gDebugPrintBuffer);
	}
	else
	 {
#if (defined (USE_SYSTEMD_JOURNAL_PRINT) || defined (USE_SYSLOG_HELPER_PRINT))
#ifdef USE_SYSTEMD_JOURNAL_PRINT

		sd_journal_print(LOG_NOTICE, "%s", gDebugPrintBuffer);
#else
		send_logs_to_syslog(gDebugPrintBuffer);
#endif
#else
		struct timeval t;
		gettimeofday(&t, NULL);
		printf("[AAMP-JS]%ld:%3ld : %s\n", (long int)t.tv_sec, (long int)t.tv_usec / 1000, gDebugPrintBuffer);
#endif
	}
}
