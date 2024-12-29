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

#ifndef __PERSISTENT_WATERMARK_EVENT_HANDLER_H__
#define __PERSISTENT_WATERMARK_EVENT_HANDLER_H__

#include <JavaScriptCore/JavaScript.h>
#include <mutex>
#include <string>
#include <vector>

namespace PersistentWatermark
{
	/**
	 @brief Encapsulates watermark Event handling
	**/
	class EventHandler
	{
	public:
		enum class eventType
		{
			ShowSuccess,
			ShowFailed,
			HideSuccess,
			HideFailed,
			size,	//not a real event, just gives the number of valid events
			null 	//represents invalid event
		};

		/**
		 @brief Send an event of the specified type with an optional message
		**/
		void Send(eventType event, std::string msg="");

		/**
		 @brief Add a callback for the specified event
		**/
		bool addEventHandler(std::string eventName, JSObjectRef callback, JSContextRef context);

		/**
		 @brief Remove callbacks for the specified event
		**/
		int RemoveEventHandler(std::string eventName);

		/**
		 @brief Return a reference to the singleton instance
		**/
		static EventHandler& getInstance()
		{
			static EventHandler instance;
			return instance;
		}

		//singleton, no copy, move or assignment
		EventHandler(const EventHandler&) = delete;
		EventHandler(EventHandler&&) = delete;
		EventHandler& operator=(const EventHandler&) = delete;
		EventHandler& operator=(EventHandler&&) = delete;

	private:
		struct callbackData
		{
			JSObjectRef callback;
			JSContextRef context;
		};

		std::vector<callbackData> mCallbacks[static_cast<int>(eventType::size)];
		std::mutex mMutex;

		/**
		 @brief Return the name of the specified event
		**/
		static std::string getEventName(eventType type);

		/**
		 @brief Return a type corresponding to the supplied event name if one exists, otherwise return eventType::null
		**/
		static eventType getEventType(std::string eventName);

		/**
		 @brief true if the event is a valid event i.e. not eventType::null or eventType::size
		**/
		static bool isValidEvent(eventType event);

		/**
		 @brief private constructor for singleton class
		**/
		EventHandler();
	};
};
#endif
