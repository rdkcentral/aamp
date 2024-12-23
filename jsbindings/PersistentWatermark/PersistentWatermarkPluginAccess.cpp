/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#ifdef USE_WATERMARK_JSBINDINGS
#include "PersistentWatermarkPluginAccess.h"

PersistentWatermark::PluginAccess::PluginAccess(): mThunderAccess("org.rdk.Watermark.1")
{
	if(mThunderAccess.ActivatePlugin())
	{
		LOG_TRACE("PersistentWatermark:Thunder activated");
	}
	else
	{
		LOG_ERROR_EX("PersistentWatermark:could not activate Thunder.");
	}
}


bool PersistentWatermark::PluginAccess::InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime)
{
	return mThunderAccess.InvokeJSONRPC(method, param, result, waitTime);
}

bool PersistentWatermark::PluginAccess::SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler)
{
	return mThunderAccess.SubscribeEvent(eventName, functionHandler);
}

bool PersistentWatermark::PluginAccess::UnSubscribeEvent (string eventName)
{
	return mThunderAccess.UnSubscribeEvent(eventName);
}

PersistentWatermark::PluginAccess& PersistentWatermark::PluginAccess::get()
{
	static PluginAccess instance;
	return instance;
}
#endif // USE_WATERMARK_JSBINDINGS

