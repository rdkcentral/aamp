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
 * @file aampcli.h
 * @brief AAMPcli header file
 */

#ifndef AAMPCLI_H
#define AAMPCLI_H

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <list>
#include <sstream>
#include <string>
#include <ctype.h>
#include <glib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <priv_aamp.h>
#include <main_aamp.h>
#include "AampConfig.h"
#include "AampDefine.h"
#include "AdManagerBase.h"
#include "StreamAbstractionAAMP.h"
#include "AampcliCommandHandler.h"
#include "AampcliVirtualChannelMap.h"
#include "AampcliGet.h"
#include "AampcliSet.h"
#include "AampcliShader.h"
#include "middleware/GstUtils.h"

class MyAAMPEventListener : public AAMPEventObjectListener
{
	public:
		const char *stringifyPlayerState(AAMPPlayerState state);
		void Event(const AAMPEventPtr& e) override;
};

class Aampcli
{
	public:
		bool mInitialized;
		bool mEnableProgressLog;
		bool mbAutoPlay;
		bool mIndexedAds = false;
		std::string mContentType;
		std::string mTuneFailureDescription;
		PlayerInstanceAAMP *mSingleton;
		MyAAMPEventListener *mEventListener;
		std::vector<PlayerInstanceAAMP *> mPlayerInstances;
		std::string mManifestDataUrl;
		GMainLoop *mAampGstPlayerMainLoop;
		GThread *mAampMainLoopThread;

		static void runCommand( std::string args );
		static gpointer aampGstPlayerStreamThread( gpointer arg );
		void doAutomation( int startChannel, int stopChannel, int maxTuneTimeS, int playTimeS, int betweenTimeS );
		FILE * getConfigFile(const std::string& cfgFile);
		void initPlayerLoop(int argc, char **argv);
		void newPlayerInstance( std::string appName = "");
		int getApplicationDir( char *buffer, uint32_t size );
		void getAdvertUrl( uint32_t reqDuration, uint32_t &adDuration, std::vector<AdvertInfo>& adList);
		
		bool SetSessionId(std::string sid);
		std::string GetSessionId() const;
		std::string GetSessionId(size_t index) const;

		Aampcli();
		Aampcli(const Aampcli& aampcli);
		Aampcli& operator=(const Aampcli& aampcli);
		~Aampcli();

	private:
		std::vector<std::string> mPlayerSessionID;
		
};

#endif // AAMPCLI_H

