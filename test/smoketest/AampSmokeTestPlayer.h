/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file AampSmokeTestPlayer.h
 * @brief AampSmokeTestPlayer header file
 */

#ifndef AAMPSMOKETESTPLAYER_H
#define AAMPSMOKETESTPLAYER_H

#include <gst/gst.h>
#include <main_aamp.h>
#include "priv_aamp.h"
#include <vector>
#include "ScriptedSmokeTestEventListener.h"


class AampPlayerInstance
{
	public:
		AampPlayerInstance():
			mPlayerInstance(NULL),
			mEventListener(NULL)
		{
		}

		AampPlayerInstance(PlayerInstanceAAMP *playerInstance, SmokeTestEventListener *eventListener):
			mPlayerInstance(playerInstance),
			mEventListener(eventListener)
		{
			if (mPlayerInstance && mEventListener)
			{
				mPlayerInstance->RegisterEvents(mEventListener);
			}
			else
			{
			}
		}

		~AampPlayerInstance()
		{
		}

		AampPlayerInstance(const AampPlayerInstance& s) = default;
		AampPlayerInstance& operator=(AampPlayerInstance const&) = delete;

		void clear()
		{
			if (mEventListener)
			{
				if (mPlayerInstance)
				{
					mPlayerInstance->UnRegisterEvents(mEventListener);
				}
				delete mEventListener;
				mEventListener = NULL;
			}
			if (mPlayerInstance)
			{
				mPlayerInstance->detach();
				delete mPlayerInstance;
				mPlayerInstance = NULL;
			}
		}

		PlayerInstanceAAMP *mPlayerInstance;
		SmokeTestEventListener *mEventListener;
};

class AampPlayer
{
	private:
		static std::vector<AampPlayerInstance> mPlayers;

	public:
		static bool mInitialized;
		bool mEnableProgressLog;
		std::string mTuneFailureDescription;
		static GMainLoop *mAampGstPlayerMainLoop;
		static GThread *mAampMainLoopThread;
		static SmokeTestEventListener *mEventListener;
		static PlayerInstanceAAMP *mPlayerInstanceAamp;
		static gpointer aampGstPlayerStreamThread( gpointer arg);
		void initPlayerLoop(int argc, char **argv);
		FILE * getConfigFile(const std::string& cfgFile);
		AampPlayer();
		AampPlayer(const AampPlayer& aampPlayer);
		AampPlayer& operator=(const AampPlayer& aampPlayer);

		int newPlayer();
		void resetPlayers();
		void deletePlayer(uint32_t index);
		PlayerInstanceAAMP *getPlayer(uint32_t index);
		SmokeTestEventListener *getListener(uint32_t index);
};


#endif // AAMPSMOKETESTPLAYER_H
