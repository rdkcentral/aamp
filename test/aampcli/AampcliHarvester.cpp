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
 * @file AampcliHarvester.cpp
 * @brief Stand alone AAMP player with command line interface.
 */

#include <unistd.h>
#include <exception>
#include "AampcliHarvester.h"
#include "AampcliPlaybackCommand.h"

Harvester::Harvester() : mHarvestThreadID()
{}

// Class statics
PlayerInstanceAAMP * Harvester::mPlayerInstanceAamp = NULL;

Harvester mHarvester;

#define HARVEST_CMD_LEN (8) // "harvest "

bool Harvester::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char harvestCmd[mHarvestCommandLength] = {'\0'};

	Harvester::mPlayerInstanceAamp = playerInstanceAamp;

	auto len = strlen(cmd);
	if ((int)len <= HARVEST_CMD_LEN)
	{
		printf("[AAMPCLI] Incomplete harvest command '%s' Check help for harvest command usage\n", cmd);
		return true;
	}
	
	strncpy(harvestCmd, cmd+HARVEST_CMD_LEN, len-HARVEST_CMD_LEN);

	printf("%s:%d: thread create: Harvester thread\n", __FUNCTION__, __LINE__);
	try {
		mHarvestThreadID =  std::thread (&Harvester::Harvest, harvestCmd);
	}catch (std::exception& e)
	{
		printf("%s:%d: Error at  thread create:Harvester thread : %s\n", __FUNCTION__, __LINE__, e.what());
	}

	if (mHarvestThreadID.joinable())
	{
		mHarvestThreadID.join();
	}

	return true;
}

void Harvester::Harvest(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item,harvestUrl;
	std::string command(cmd);
	time_t initialTime, currentTime;

	if(aamp_pthread_setname(pthread_self(), "Harvester"))
	{
		printf("%s:%d: Harvester_thread aamp_pthread_setname failed\n",__FUNCTION__,__LINE__);
	}

	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}

	if (command.find("harvestUrl=") == std::string::npos)
	{
		printf("%s:%d: Harvest url is not found in command '%s' Check help for harvest command usage\n",__FUNCTION__,__LINE__,command.c_str());
		return;
	}
	
	for(std::string param : params)
	{
		std::size_t delimiterPos = param.find("=");

		if(delimiterPos != std::string::npos)
		{
			mHarvester.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
			mHarvester.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
		else
		{
			printf("%s:%d: Error in command param %s\n",__FUNCTION__,__LINE__,param.c_str());
			return;
		}

	}

	//Set abr to false, to download content from single profile
	mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_EnableABR, false);

	if (mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestPath) == "")
	{
		printf("%s:%d: Harvest path is not found, contents will be downloaded in default path \n",__FUNCTION__,__LINE__);
	}

	if (!mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig))
	{
		printf("%s:%d: Harvest config is not found in command '%s' Check help for harvest command usage\n",__FUNCTION__,__LINE__,command.c_str());
		return;
	}

	//When harvest count is not set, count is set to default value
	if(mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit) <= 0)
	{
		mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestCountLimit, DEFAULT_HARVEST_COUNT_LIMIT);
	}

	int harvestConfig = 0;
	harvestConfig = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig) ;

	mHarvester.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

	if ((harvestConfig & getHarvestConfigForMedia(eMEDIATYPE_IFRAME)) != 0)
	{
		float rate = 2;
		mHarvester.mPlayerInstanceAamp->SetRate(rate);

	}
	else if((command.find("trackId=") != std::string::npos) && (getHarvestConfigForMedia(eMEDIATYPE_AUDIO)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvester.mPlayerInstanceAamp->SetAudioTrack(trackId);
		printf("%s:%d: Harvest audio trackId %d\n",__FUNCTION__,__LINE__,trackId);
	}
	else if((command.find("trackId=") != std::string::npos) && (getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvester.mPlayerInstanceAamp->SetCCStatus(true);
		mHarvester.mPlayerInstanceAamp->SetTextTrack(trackId);
		printf("%s:%d: Harvest subtitle trackId %d\n",__FUNCTION__,__LINE__,trackId);
	}

	int harvestDuration = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestDuration);
	initialTime = time(NULL);
	
	while (1)
	{
		sleep(1);

		if(mHarvester.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("%s:%d: Tune completed exiting harvesting mode\n",__FUNCTION__,__LINE__);
			break;
		}

		if (mHarvester.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING)
		{
			mHarvester.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

			if ((harvestConfig & getHarvestConfigForMedia(eMEDIATYPE_IFRAME)) != 0)
			{
				float rate = 2;
				mHarvester.mPlayerInstanceAamp->SetRate(rate);
				
			}
		}
		
		int currHarvestCount = mHarvester.mPlayerInstanceAamp->aamp->GetHarvestRemainingFragmentCount();
		if (currHarvestCount == 0)
		{
			printf("%s:%d: Harvest count completed, exiting harvesting mode.\n",__FUNCTION__,__LINE__);
			mHarvester.mPlayerInstanceAamp->Stop();
			sleep(5);  // allow logs to propogate to data output
			break;
		}

		// Don't use duration if not configured
		currentTime = time(NULL);
	
		if ((harvestDuration > 0) && (currentTime - initialTime) > harvestDuration)
		{
			printf("%s:%d: Harvest time completed, exiting harvest mode.\n",__FUNCTION__,__LINE__);
			mHarvester.mPlayerInstanceAamp->Stop();
			sleep(5); // allow logs to propagate to data output
			break;
		}
	}
	
	return;
}

