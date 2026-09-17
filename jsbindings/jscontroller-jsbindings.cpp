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
 * @file jscontroller-jsbindings.cpp
 * @brief JavaScript bindings for AAMP_JSController
 */


#include <JavaScriptCore/JavaScript.h>

#include "jsbindings.h"
#include "jsutils.h"
#include "priv_aamp.h"

#include <stdio.h>
#include <string.h>

static std::string g_UserAgent;
extern "C"
{
	void aamp_LoadJSController(JSGlobalContextRef context);

	void aamp_UnloadJSController(JSGlobalContextRef context);

	void aamp_SetPageHttpHeaders(const char* headers);

	void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP *);	
}
//global variable to hold the custom http headers
static std::map<std::string, std::string> g_PageHttpHeaders;

/**
 * @brief Sets the custom http headers from received json string
 * @param[in] headerJson http headers json in string format
 */
void aamp_SetPageHttpHeaders(const char* headerJson)
{
	//headerJson is expected to be in the format "[{\"name\":\"X-PRIVACY-SETTINGS\",\"value\":\"lmt=1,us_privacy=1-Y-\"},    {\"name\":\"abc\",\"value\":\"xyz\"}]"
	g_PageHttpHeaders.clear();
	if(nullptr != headerJson && '\0' != headerJson[0])
	{
		LOG_WARN_EX("aamp_SetPageHttpHeaders headerJson=%s", headerJson);
		cJSON *parentJsonObj = cJSON_Parse(headerJson);
		cJSON *jsonObj = nullptr;
		if(nullptr != parentJsonObj)
		{
			jsonObj = parentJsonObj->child;
		}
		
		while(nullptr != jsonObj)
		{
			cJSON *child = jsonObj->child;
			std::string key = "";
			std::string val = "";
			while( nullptr != child )
			{
				if(strcmp (child->string,"name") == 0)
				{
					key = std::string(child->valuestring);
				}
				else if(strcmp (child->string,"value") == 0)
				{
					val = std::string(child->valuestring);
				}
				child = child->next;
			}
			if(!key.empty() && key == "X-PRIVACY-SETTINGS")/* Condition is added to filter out header "X-PRIVACY-SETTINGS" since other headers like X-Forwarded-For/User-Agent/Accept-Language/ are appearing in case of x1 **/
			{
				//insert key value pairs one by one
				g_PageHttpHeaders.insert(std::make_pair(key, val));
			}
			jsonObj = jsonObj->next;
		}
		if(parentJsonObj)
			cJSON_Delete(parentJsonObj);
	}
}
/**
 * @brief applies the parsed custom http headers into the aamp
 * @param[in] aampObject main aamp object
 */
void aamp_ApplyPageHttpHeaders(PlayerInstanceAAMP * aampObject)
{
	LOG_WARN_EX("aamp_ApplyPageHttpHeaders aampObject=%p", aampObject);
	if(NULL != aampObject)
	{
		aampObject->AddPageHeaders(g_PageHttpHeaders);
	}
}

/**
 * @brief Loads AAMP_JSController JS object into JS execution context
 * @param[in] context JS execution context
 */
void aamp_LoadJSController(JSGlobalContextRef context)
{
	LOG_WARN_EX("aamp_LoadJSController context=%p", context);
	g_UserAgent = "";

	// For this ticket we have repurposed AAMP.version to return the UVE bindings version
	// When at some point in future we deprecate legacy bindings or aamp_LoadJS() please don't forget
	// to retain this support to avoid backward compatibility issues.
	aamp_LoadJS(context, NULL);
	AAMPPlayer_LoadJS(context);
}


/**
 * @brief Removes the AAMP_JSController instance from JS context
 * @param[in] context JS execution context
 */
void aamp_UnloadJSController(JSGlobalContextRef context)
{
	LOG_WARN_EX("aamp_UnloadJSController context=%p", context);

	aamp_UnloadJS(context);
	AAMPPlayer_UnloadJS(context);

	LOG_TRACE("JSGarbageCollect(%p)", context);
	// Force garbage collection to free up memory
	JSGarbageCollect(context);
}


/**
 * @brief Read userAgent from browser
 * @param[in] context JS execution context
 */
std::string GetBrowserUA(JSContextRef ctx)
{
	//Read only when g_UserAgent is empty
	if(g_UserAgent.empty())
	{
		const char* userAgentStr = "window.navigator.userAgent";
		JSStringRef script = JSStringCreateWithUTF8CString(userAgentStr);
		JSValueRef propName = JSEvaluateScript(ctx, script, NULL, NULL, 0, NULL);
		if(propName != NULL)
		{
			if (JSValueIsString(ctx, propName))
			{
				char* value = aamp_JSValueToCString(ctx, propName, NULL);
				LOG_WARN_EX("Parsed value for property %s - %s","window.navigator.userAgent", value);
				// Setting user agent to global variable
				g_UserAgent = std::string(value);
			}
		}
		else
		{
			LOG_ERROR_EX("Invalid value for property %s passed","window.navigator.userAgent");
		}
		JSStringRelease(script);
	}
	return g_UserAgent;
}
