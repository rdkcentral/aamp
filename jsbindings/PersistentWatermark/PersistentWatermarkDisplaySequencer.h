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
#ifndef __PERSISTENT_WATERMARK_DISPLAY_SEQUENCER__H__
#define __PERSISTENT_WATERMARK_DISPLAY_SEQUENCER__H__

#include "PersistentWatermarkEventHandler.h"
#include "PersistentWatermarkStorage.h"

namespace PersistentWatermark
{
	/**
	 @brief Controls the watermark plugin
	* */
	class DisplaySequencer
	{

	public:

		/**
		*   @brief Show the Watermark contained in storage
		*   @param[in]  storage - storage object containing the watermark to display
		*   @param[in]  opacity - opacity of the watermark (0-100) this is basically defines % of alpha w.r.t to original alpha in pixel
		*/
		void Show(PersistentWatermark::Storage& storage, int opacity=100);

		/**
		 @brief Hide any watermark currently displayed
		* */
		void Hide();

		/**
		 @brief Return a reference to the singleton instance
		**/
		static DisplaySequencer& getInstance()
		{
			static DisplaySequencer instance;
			return instance;
		}

		~DisplaySequencer();

		//singleton, no copy, move or assignment
		DisplaySequencer(const DisplaySequencer&) = delete;
		DisplaySequencer(DisplaySequencer&&) = delete;
		DisplaySequencer& operator=(const DisplaySequencer&) = delete;
		DisplaySequencer& operator=(DisplaySequencer&&) = delete;

	private:
		int mCurrentLayerID;

		/**
		 @brief private constructor for singleton class
		**/
		DisplaySequencer();

		/**
		 @brief convenience function for deleting a watermark layer
		**/
		void deleteLayer();

		/**
		 @brief Send an event
		**/
		void SendEvent(EventHandler::eventType event, std::string msg="");
	};
};
#endif
