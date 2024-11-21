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

#ifndef __PERSISTENT_WATERMARK_PLUGIN_ACCESS__H__
#define __PERSISTENT_WATERMARK_PLUGIN_ACCESS__H__

#ifdef USE_CPP_THUNDER_PLUGIN_ACCESS
#include "ThunderAccess.h"
#include "jsutils.h"

namespace PersistentWatermark
{
	/**
	 @brief Controls access to the watermark plugin
	* */
	class PluginAccess
	{

	public:
		/**
		 @brief get Singleton instance
		*/
		static PluginAccess& get();

		//Provide the ThunderAccessAAMP interface
		bool InvokeJSONRPC(std::string method, const JsonObject &param, JsonObject &result, const uint32_t waitTime = THUNDER_RPC_TIMEOUT);
		bool SubscribeEvent (string eventName, std::function<void(const WPEFramework::Core::JSON::VariantContainer&)> functionHandler);
		bool UnSubscribeEvent (string eventName);

		//singleton, no copy, move or assignment
		PluginAccess(const PluginAccess&) = delete;
		PluginAccess(PluginAccess&&) = delete;
		PluginAccess& operator=(const PluginAccess&) = delete;
		PluginAccess& operator=(PluginAccess&&) = delete;

	private:

		ThunderAccessAAMP mThunderAccess;
		/**
		 @brief private constructor for singleton class
		**/
		PluginAccess();
	};
};
#endif // USE_CPP_THUNDER_PLUGIN_ACCESS
#endif // __PERSISTENT_WATERMARK_PLUGIN_ACCESS__H__

