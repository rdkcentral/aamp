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

Harvester::Harvester() : mMasterHarvesterThreadID(),
			mSlaveHarvesterThreadID(),
			mReportThread(),
			mSlaveVideoThreads{},
			mSlaveAudioThreads{},
			mSlaveSubtitleThreads{},
			mSlaveIFrameThread(),
			mExePathName(""),
			mHarvestCountLimit(0),
			mHarvestDuration(0),
			mHarvestConfig(65535),
			mTCPServerSinkPort(0)
{}

// Class statics
std::mutex Harvester::mHarvestInfoMutex;
std::map<std::thread::id, HarvestProfileDetails> Harvester::mHarvestInfo = std::map<std::thread::id, HarvestProfileDetails>();
std::vector<std::thread::id> Harvester::mHarvestThreadId(0);
PlayerInstanceAAMP * Harvester::mPlayerInstanceAamp = NULL;
bool Harvester::mHarvestReportFlag = false;
std::string Harvester::mHarvestPath = "";

Harvester mHarvester;

#define HARVEST_CMD_LEN (8) // "harvest "

bool Harvester::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char harvestCmd[mHarvestCommandLength] = {'\0'};

	Harvester::mPlayerInstanceAamp = playerInstanceAamp;

	auto len = strlen(cmd);
	if ((int)len <= HARVEST_CMD_LEN)
	{
		printf("[AAMPCLI] Incomplete harvest command '%s'\n", cmd);
		return true;
	}
	
	strncpy(harvestCmd, cmd+HARVEST_CMD_LEN, len-HARVEST_CMD_LEN);
	
	if( PlaybackCommand::isCommandMatch(harvestCmd,"harvestMode=Master") )
	{
		printf("thread create:MasterHarvester thread\n");
		try {
			mMasterHarvesterThreadID = std::thread (&Harvester::masterHarvester, harvestCmd);
		}
		catch (std::exception& e)
		{
			printf("Error at thread create:MasterHarvester thread : %s\n", e.what());
		}
	}
	else if( PlaybackCommand::isCommandMatch(harvestCmd,"harvestMode=Slave") )
	{
		printf("thread create:SlaveHarvester thread\n" );
		try {
			mSlaveHarvesterThreadID =  std::thread (&Harvester::slaveHarvester, harvestCmd);
		}catch (std::exception& e)
		{
			printf("Error at  thread create:SlaveHarvester thread : %s\n", e.what());
		}
	}
	else
	{
		printf("[AAMPCLI] Invalid harvest command '%s'\n", cmd);
	}

	if (mMasterHarvesterThreadID.joinable())
	{
		mMasterHarvesterThreadID.join();
	}

	if (mSlaveHarvesterThreadID.joinable())
	{
		mSlaveHarvesterThreadID.join();
	}

	return true;
}

std::vector<AudioTrackInfo> Harvester::GetAudioTracks()
{
	std::vector<AudioTrackInfo> audioTrackVec;
	std::string json = mHarvester.mPlayerInstanceAamp->GetAvailableAudioTracks(true);

	cJSON *root = cJSON_Parse(json.c_str());
	if (root)
	{
		cJSON *arrayItem = NULL;
		int arraySize = cJSON_GetArraySize(root);
		for ( int item = 0; item < arraySize; item++)
		{
			// As we don't get any sort of index, we'll just try to create a unique value for the item
			arrayItem = cJSON_GetArrayItem(root, item);
			std::string arrayItemText = cJSON_Print(arrayItem);
			std::size_t itemHash = std::hash<std::string>{}(arrayItemText);

			// Just fill the fields we require
			AudioTrackInfo trackInfo;
			trackInfo.index = std::to_string(itemHash);
			cJSON *bandwidth = cJSON_GetObjectItem(arrayItem, "bandwidth");
			if (bandwidth) // not all tracks have this field
			{
				trackInfo.bandwidth = bandwidth->valuedouble;
			}
			trackInfo.language = cJSON_GetObjectItem(arrayItem, "language")->valuestring;
			audioTrackVec.push_back(trackInfo);
		}
		cJSON_Delete(root);
	}

	return audioTrackVec;
}

std::vector<TextTrackInfo> Harvester::GetTextTracks()
{
	std::vector<TextTrackInfo> textTrackVec;
	std::string json = mHarvester.mPlayerInstanceAamp->GetAvailableTextTracks(true);

	cJSON *root = cJSON_Parse(json.c_str());
	if (root)
	{
		cJSON *arrayItem = NULL;
		int arraySize = cJSON_GetArraySize(root);
		for ( int item = 0; item < arraySize; item++)
		{
			// Check to see if any of the tracks are closed captions
			bool isCC = false;
			arrayItem = cJSON_GetArrayItem(root, item);
			cJSON *jsonData = arrayItem->child;
			while (jsonData)
			{
				if (std::string("accessibility") == jsonData->string) 
				{
					jsonData = jsonData->child;
					while (jsonData)
					{
						if ((std::string("scheme") == jsonData->string) &&
							(jsonData->type == cJSON_String))
						{
							isCC = (std::string(jsonData->valuestring).substr(0,20) == "urn:scte:dash:cc:cea"); // check the scheme for cc data
							break;
						}
						jsonData = jsonData->next;
					}
					break;
				}
				jsonData = jsonData->next;
			}

			if (isCC)
			{
				// We can't harvest inband data
				continue;
			}

			// As we don't get any sort of index, we'll just try to create a unique value for the item
			std::string arrayItemText = cJSON_Print(arrayItem);
			std::size_t itemHash = std::hash<std::string>{}(arrayItemText);

			// Just fill the fields we require
			TextTrackInfo trackInfo;
			trackInfo.index = std::to_string(itemHash);
			trackInfo.language = cJSON_GetObjectItem(arrayItem, "language")->valuestring;
			textTrackVec.push_back(trackInfo);
		}
		cJSON_Delete(root);
	}

	return textTrackVec;
}


void Harvester::masterHarvester(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item;
	FILE * pIframe;
	int videoThreadId = 0;
	int audioThreadId = 0;
	int subtitleThreadId = 0;
	std::string masterCmd = "harvest harvestMode=Master ";
	// Save off master values we end up changing for later restore
	int restoreMasterConfig = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig);
	int restoreMasterCountLimit = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit);

	if(aamp_pthread_setname(pthread_self(), "MasterHarvester"))
	{
		printf("aamp_pthread_setname failed\n" );
	}

	mHarvester.getExecutablePath();
	
	// NOTE: Expect aamp.cfg or passed parameters to include useTCPServerSink / suppressDecode as appropiate not set here!

	/*
	** Process harvest command line parameters
	*/
	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}

	for(std::string param : params)
	{
		std::size_t delimiterPos = param.find("=");

		if((delimiterPos != std::string::npos) && (param.substr(delimiterPos + 1) != ""))
		{
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
		else
		{
			printf("Error in command param %s\n", param.c_str());
			return;
		}
		// Set master harvestPath if passed in as parameter
		if (param.find("harvestPath") != std::string::npos)
		{
			mHarvester.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
			mHarvester.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);
			masterCmd += param;
		}
	}
	
	// See if a harvestConfig is provided, priority: set by harvest command option, set by aamp.cfg, default if not set. Master and slave will divvy up different parts of harvestConfig.
	if(cmdlineParams.find("harvestConfig") != cmdlineParams.end())
	{
		mHarvester.mHarvestConfig = (int)mHarvester.getNumberFromString(cmdlineParams["harvestConfig"].c_str());
	}
	else
	{
		mHarvester.mHarvestConfig = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig);
		if (mHarvester.mHarvestConfig == 0)
		{
			// getHarvestConfigForMedia() has no value to return eHARVEST_ENABLE_DEFAULT and not present in header file so we'll just set it here.
			mHarvester.mHarvestConfig = 0xFFFFFFFF;
		}
	}
	
	// Master harvests some limited stream types (manifest, playlists)
	unsigned int harvestConfig =
		getHarvestConfigForMedia(eMEDIATYPE_MANIFEST) |
		getHarvestConfigForMedia(eMEDIATYPE_PLAYLIST_VIDEO) |
		getHarvestConfigForMedia(eMEDIATYPE_PLAYLIST_AUDIO) |
		getHarvestConfigForMedia(eMEDIATYPE_PLAYLIST_SUBTITLE) |
		getHarvestConfigForMedia(eMEDIATYPE_PLAYLIST_IFRAME) |
		getHarvestConfigForMedia(eMEDIATYPE_DSM_CC);
	// Remove any not listed in mHarvestConfig
	harvestConfig &= mHarvester.mHarvestConfig;
	
	// Set master harvestConfig
	mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, harvestConfig);
	masterCmd += " harvestConfig=" + std::to_string(harvestConfig);

	

	/*
	** Determine slave harvester settings
	*/
	
	mHarvester.mHarvestDuration = 0;
	// Checking for conflicting harvestDuration & harvestCountLimit
	if(cmdlineParams.find("harvestDuration") != cmdlineParams.end())
	{
		mHarvester.mHarvestDuration = (int)mHarvester.getNumberFromString(cmdlineParams["harvestDuration"].c_str());
	}
	else
	{
		mHarvester.mHarvestDuration = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestDuration);
	}

	mHarvester.mHarvestCountLimit = 0;
	if(cmdlineParams.find("harvestCountLimit") != cmdlineParams.end())
	{
		mHarvester.mHarvestCountLimit = (int)mHarvester.getNumberFromString(cmdlineParams["harvestCountLimit"].c_str());
	}
	else
	{
		mHarvester.mHarvestCountLimit = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit);
	}
	
	// Set master harvestCountLimit to a large value, let the slave harvests dictate when master should end harvesting
	mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestCountLimit, DEFAULT_HARVEST_COUNT_LIMIT);
	masterCmd += " harvestCountLimit=" + std::to_string(DEFAULT_HARVEST_COUNT_LIMIT);

	// We're trying to detect a user set harvest count limit and harvest duration, which is not allowed.  We always need a harvestCountLimit to do any harvesting, just not wanting user to specify both.
	if(mHarvester.mHarvestDuration > 0 && (mHarvester.mHarvestCountLimit > 0 && mHarvester.mHarvestCountLimit != DEFAULT_HARVEST_COUNT_LIMIT))
	{
		printf(" %s:%d: Error harvestDuration and harvestCountLimit both configured.\n", __FILE__, __LINE__);
		return;
	}
	
	// Make sure there is a valid mHarvestCountLimit as slaves won't harvest if it goes to 0 even when using duration.
	if (mHarvester.mHarvestDuration <= 0)
	{
		if (mHarvester.mHarvestCountLimit == 0)
		{
			mHarvester.mHarvestCountLimit = DEFAULT_HARVEST_COUNT_LIMIT;
		}
	}

	else // if there is a duration, then set a high mHarvestCountLimit
	{
		mHarvester.mHarvestCountLimit = DEFAULT_HARVEST_COUNT_LIMIT;
	}
	
	// Harvester will maintain unique TCPServerSinkPort if enabled
	mHarvester.mUseTCPServerSink = mHarvester.mPlayerInstanceAamp->mConfig.IsConfigSet(eAAMPConfig_useTCPServerSink);
	mHarvester.mTCPServerSinkPort = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_TCPServerSinkPort);

	if (mHarvester.mUseTCPServerSink && mHarvester.mTCPServerSinkPort > 0)
	{
		masterCmd += " useTCPServerSink=1";
		masterCmd += " TCPserverSinkPort=" + std::to_string(mHarvester.mTCPServerSinkPort);
		mHarvester.mTCPServerSinkPort += 2;	// increment for next aamp-cli as port already used on master tune
	}
	mHarvester.mSuppressDecode = mHarvester.mPlayerInstanceAamp->mConfig.IsConfigSet(eAAMPConfig_SuppressDecode);
	if (mHarvester.mSuppressDecode)
	{
		masterCmd += " suppressDecode=1";
	}
	
	
	// Tune and launch slave harvesters
	if(cmdlineParams.find("harvestUrl") != cmdlineParams.end())
	{
		masterCmd += " harvestUrl=" + cmdlineParams["harvestUrl"];
		printf("Config process of master harvest command %s \n", masterCmd.c_str());
		
		mHarvester.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

		Harvester::mHarvestPath = cmdlineParams["harvestPath"];

		try
		{
			mHarvester.writeHarvestReport((char *)masterCmd.c_str(), true);
		}
		catch (std::exception& e)
		{
			printf("Error at startHarvestReport : %s\n", e.what());
		}
		
		// if iframe enabled, then harvest
		if ((mHarvester.mHarvestConfig & getHarvestConfigForMedia(eMEDIATYPE_IFRAME)) != 0)
		{
			harvestConfig = getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_IFRAME);
			// Remove any not listed in mHarvestConfig
			harvestConfig &= mHarvester.mHarvestConfig;
			
			pIframe = mHarvester.createSlaveHarvester (cmdlineParams, harvestConfig);
			if (pIframe != NULL)
			{
				mHarvester.createSlaveDataReader(pIframe, mHarvester.mSlaveIFrameThread);
			}
			else
			{
				printf("could not create slave harvest process for iframe." );
			}
		}

		std::vector<long> cacheVideoBitrates;
		std::vector<AudioTrackInfo> cacheAudioTrackInfo;
		std::vector<TextTrackInfo> cacheTextTrackInfo;

		while(1)
		{

			std::vector<long> tempVideoBitrates,diffVideoBitrates;
			std::vector<AudioTrackInfo> currentAudioTrackInfo,diffAudioTrackInfo;
			std::vector<TextTrackInfo> currentTextTrackInfo,diffTextTrackInfo;
			sleep (1);

			if(mHarvester.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
			{
				sleep(5); //Waiting for slave process to complete
			}

			if ((mHarvester.mPlayerInstanceAamp->GetState() != eSTATE_COMPLETE) && (mHarvester.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING))
			{
				printf("Tune stopped\n" );
				mHarvester.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());
			}

			/*
			 ** Determine what is available to be harvested. Only harvest what has been requested.
			 */
			
			if ((mHarvester.mHarvestConfig & getHarvestConfigForMedia(eMEDIATYPE_VIDEO)) != 0)
			{
				tempVideoBitrates = mHarvester.mPlayerInstanceAamp->GetVideoBitrates();
				std::sort(tempVideoBitrates.begin(), tempVideoBitrates.end());
			}
			if ((mHarvester.mHarvestConfig & getHarvestConfigForMedia(eMEDIATYPE_AUDIO)) != 0)
			{
				currentAudioTrackInfo = mHarvester.GetAudioTracks();
			}
			if ((mHarvester.mHarvestConfig & getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE)) != 0)
			{
				currentTextTrackInfo = mHarvester.GetTextTracks();
			}
						
			std::vector<long>::iterator position = std::find(tempVideoBitrates.begin(), tempVideoBitrates.end(), mHarvester.mPlayerInstanceAamp->GetVideoBitrate());
			if (position != tempVideoBitrates.end())
			{
				tempVideoBitrates.erase(position);
			}

			if (cacheVideoBitrates.size() == 0)
			{
				diffVideoBitrates = tempVideoBitrates;
			}
			else
			{
				for (long tempVideoBitrate: tempVideoBitrates)
				{
					if ( ! (std::find(cacheVideoBitrates.begin(), cacheVideoBitrates.end(), tempVideoBitrate) != cacheVideoBitrates.end()) )
					{
						diffVideoBitrates.push_back(tempVideoBitrate);
					}
				}
			}

			if (cacheAudioTrackInfo.size() == 0)
			{
				diffAudioTrackInfo = currentAudioTrackInfo;
			}
			else
			{
				if(cacheAudioTrackInfo.size() < currentAudioTrackInfo.size())
				{
					for(auto cItr = currentAudioTrackInfo.begin(); cItr != currentAudioTrackInfo.end(); cItr++)
					{
						bool status = false;
						for(auto pItr = cacheAudioTrackInfo.begin(); pItr!= cacheAudioTrackInfo.end(); pItr++)
						{
							if(cItr->index == pItr->index)
							{
								status = true;
							}
						}
						
						if(status == false)
							diffAudioTrackInfo.push_back(*cItr);
					}
				}
			}

			if ((cacheTextTrackInfo.size() == 0)  && (!currentTextTrackInfo.empty()))
			{
				diffTextTrackInfo = currentTextTrackInfo;
			}
			else
			{
				if(cacheTextTrackInfo.size() < currentTextTrackInfo.size())
				{
					for(auto cItr = currentTextTrackInfo.begin(); cItr != currentTextTrackInfo.end(); cItr++)
					{
						bool status = false;
						for(auto pItr = cacheTextTrackInfo.begin(); pItr!= cacheTextTrackInfo.end(); pItr++)
						{
							if(cItr->index == pItr->index)
							{
								status = true;
							}
						}
						
						if(status == false)
							diffTextTrackInfo.push_back(*cItr);
					}
				}
			}
			
			/*
			 ** Create harvesters
			 */

			if(diffVideoBitrates.size())
			{
				for(auto bitrate:diffVideoBitrates)
				{
					harvestConfig = getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_LICENCE);
					// Remove any not listed in mHarvestConfig
					harvestConfig &= mHarvester.mHarvestConfig;
					
					FILE *p = NULL;
					p = mHarvester.createSlaveHarvester (cmdlineParams, harvestConfig, bitrate);
					if (p != NULL)
					{
						if (mHarvester.createSlaveDataReader(p, mHarvester.mSlaveVideoThreads[videoThreadId]) == true)
						{
							videoThreadId++;
						}
					}
					else
					{
						printf("could not create slave harvest process for video." );
					}
				}
				cacheVideoBitrates = tempVideoBitrates;
			}

			if(diffAudioTrackInfo.size())
			{
				for(int trackId = 0; trackId < diffAudioTrackInfo.size(); trackId++)
				{
					harvestConfig = getHarvestConfigForMedia(eMEDIATYPE_AUDIO)|getHarvestConfigForMedia(eMEDIATYPE_INIT_AUDIO);
					// Remove any not listed in mHarvestConfig
					harvestConfig &= mHarvester.mHarvestConfig;
					
					FILE *p = NULL;
					
					p = mHarvester.createSlaveHarvester (cmdlineParams, harvestConfig,
					                                     diffAudioTrackInfo[trackId].bandwidth,
														 diffAudioTrackInfo[trackId].language, trackId);
					if (p != NULL)
					{
						if (mHarvester.createSlaveDataReader(p, mHarvester.mSlaveAudioThreads[audioThreadId]) == true)
						{
							audioThreadId++;
						}
					}
					else
					{
						printf("could not create slave harvest process for audio." );
					}
				}

				cacheAudioTrackInfo = currentAudioTrackInfo;
			}

			if(diffTextTrackInfo.size())
			{
				for(int trackId = 0; trackId < diffTextTrackInfo.size(); trackId++)
				{
					harvestConfig = getHarvestConfigForMedia(eMEDIATYPE_INIT_SUBTITLE)|getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE);
					// Remove any not listed in mHarvestConfig
					harvestConfig &= mHarvester.mHarvestConfig;
					
					FILE *p = NULL;
					
					p = mHarvester.createSlaveHarvester (cmdlineParams, harvestConfig,
					                                     0L,
														 diffTextTrackInfo[trackId].language, trackId);
					if (p != NULL)
					{
						if (mHarvester.createSlaveDataReader(p, mHarvester.mSlaveSubtitleThreads[subtitleThreadId]) == true)
						{
							subtitleThreadId++;
						}
					}
					else
					{
						printf("could not create slave harvest process for subtitle." );
					}
				}

				cacheTextTrackInfo = currentTextTrackInfo;
			}

			// mHarvestInfo entries should all be created at this point via createSlaveDataReader()
			{
				std::lock_guard<std::mutex> guard(mHarvestInfoMutex);
				bool done = true;
				// We are done when all of the data readers are done
				for (auto itr = mHarvestInfo.begin(); itr != mHarvestInfo.end(); itr++)
				{
					if (itr->second.harvestEndFlag == false)
					{
						done = false;
						break;
					}
				}
				if (done)
				{
					break;
				}
			}
		}

		if (mHarvester.mSlaveIFrameThread.joinable())
		{
			mHarvester.mSlaveIFrameThread.join();
		}

		for(int i = 0; i < videoThreadId; i++)
		{
			if (mHarvester.mSlaveVideoThreads[i].joinable())
			{
				mHarvester.mSlaveVideoThreads[i].join();
			}
		}

		for(int i =0; i < audioThreadId; i++)
		{
			if(mHarvester.mSlaveAudioThreads[i].joinable())
			{
				mHarvester.mSlaveAudioThreads[i].join();
			}
		}

		for(int i =0; i < subtitleThreadId; i++)
		{
			if(mHarvester.mSlaveSubtitleThreads[i].joinable())
			{
    			mHarvester.mSlaveSubtitleThreads[i].join();
            }
		}
		
		printf("all harvesting complete.\n" );

		// Clear static member data for next run
		mHarvestInfo.clear();
		mHarvestThreadId.clear();
		
		// Restore master config for another run
		mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestConfig, restoreMasterConfig);
		mHarvester.mPlayerInstanceAamp->mConfig.SetConfigValue(AAMP_DEV_CFG_SETTING, eAAMPConfig_HarvestCountLimit, restoreMasterCountLimit);
	}
	else
	{
		printf("harvestUrl not found\n" );
	}

	return;
}

void Harvester::slaveHarvester(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item,harvestUrl;
	time_t initialTime, currentTime;

	if(aamp_pthread_setname(pthread_self(), "SlaveHarvester"))
	{
		printf("SlaveHarvester_thread aamp_pthread_setname failed\n" );
	}

	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}

	params.push_back("useTCPServerSink=true");

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
			printf("Error in command param %s\n",param.c_str());
			return;
		}

	}


	int harvestConfig = 0;
	harvestConfig = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig) ;

	mHarvester.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

	if ((harvestConfig & getHarvestConfigForMedia(eMEDIATYPE_IFRAME)) != 0)
	{
		float rate = 2;
		mHarvester.mPlayerInstanceAamp->SetRate(rate);

	}
	else if(harvestConfig & (getHarvestConfigForMedia(eMEDIATYPE_AUDIO)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvester.mPlayerInstanceAamp->SetAudioTrack(trackId);
		printf("Harvest audio trackId %d\n",trackId);
	}
	else if(harvestConfig & (getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvester.mPlayerInstanceAamp->SetCCStatus(true);
		mHarvester.mPlayerInstanceAamp->SetTextTrack(trackId);
		printf("Harvest subtitle trackId %d\n",trackId);
	}

	int harvestDuration = mHarvester.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestDuration);
	initialTime = time(NULL);
	
	while (1)
	{
		sleep(1);

		if(mHarvester.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("Tune completed exiting harvesting mode\n" );
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
			printf("Harvest count completed, exiting harvesting mode.\n" );
			sleep(5);  // allow logs to propogate to slaveDataOutput
			break;
		}

		// Don't use duration if not configured
		currentTime = time(NULL);
	
		if ((harvestDuration > 0) && (currentTime - initialTime) > harvestDuration)
		{
			printf("Harvest time completed, exiting harvest mode.\n" );
			sleep(5); // allow logs to propagate to slaveDataOutput
			break;
		}
	}
	exit(0);
	return;
}

void Harvester::slaveDataOutput(void * arg)
{
	FILE * pipe;
	pipe = (FILE *) arg;
	char buffer[500] = {'\0'};
	bool active = false;
	if(aamp_pthread_setname(pthread_self(), "SlaveOutput"))
	{
		printf("aamp_pthread_setname failed\n");
	}

	while (!feof(pipe))
	{
		if (fgets(buffer, 500, pipe) != NULL)
		{
			active = mHarvester.getHarvestReportDetails(buffer);
			printf("[0x%p] %s",pipe,buffer);
		}
		
		if (active == false) // end this thread
		{
			printf("EndOfHarvestReached harvesting complete, exiting thread.\n");
			break;
		}
	}
	
	if (active)  // feof end of duration likely
	{
		strncpy(buffer,"EndOfStreamReached", sizeof(buffer)-1);
		mHarvester.getHarvestReportDetails(buffer);
		printf("EndOfHarvestReached harvesting complete, exiting thread.\n");
	}

	return;
}

long Harvester::getNumberFromString(std::string buffer)
{
	std::stringstream strm;
	strm << buffer;
	std::string tempStr;
	long value = 0;
	while(!strm.eof()) {
		strm >> tempStr;
		if(std::stringstream(tempStr) >> value)
		{
			return value;
		}

		tempStr = ""; 
	}

	return value;
}

void Harvester::writeHarvestReport(const char* harvestCmd, bool isMaster)
{
	FILE *fp, *errorfp;
	std::string filename, errorFilename;
	
	filename = Harvester::mHarvestPath + "/HarvestReport.txt";
	errorFilename = Harvester::mHarvestPath + "/HarvestErrorReport.txt";
	
	fp = fopen(filename.c_str(), isMaster ? "w" : "a");
	errorfp = fopen(errorFilename.c_str(), isMaster ? "w" : "a");
	
	if (fp == NULL || errorfp == NULL)
	{
		printf("Error opening the file %s (%s)\n", fp == NULL ? filename.c_str() : errorFilename.c_str(), strerror(errno));
		return;
	}
	
	fprintf(fp, "Harvest Profile Details\n\n");
	fprintf(fp, "Harvest %s cmd : '%s'\n", isMaster ? "Master" : "Slave", harvestCmd);
	if (isMaster)
	{
		Harvester::mHarvestReportFlag = true;
	}
	
	fprintf(errorfp, "Harvest %s cmd : '%s'\n", isMaster ? "Master" : "Slave", harvestCmd);
	
	fclose(fp);
	fclose(errorfp);
}

bool Harvester::getHarvestReportDetails(char *buffer)
{
	std::lock_guard<std::mutex> guard(mHarvestInfoMutex);
	
	char *token = NULL;
	std::thread::id threadId;
	std::map<std::thread::id, HarvestProfileDetails>::iterator itr;
	bool harvestComplete = false;

	threadId = std::this_thread::get_id();
	itr = mHarvestInfo.find(threadId);
	if (itr == mHarvestInfo.end())		// unexpected, could return false as not active?
	{
		printf("did not find thread in mHarvestInfo list.\n");
		return true;
	}

	if ((itr->second.harvestConfig == 0) && (strstr(buffer, "harvestConfig -") != NULL))
	{
		token = strstr(buffer, "harvestConfig -");
		
		itr->second.harvestConfig = (int)getNumberFromString(token);

		if(itr->second.harvestConfig == 193)
			strcpy(itr->second.media,"IFrame");

		if(itr->second.harvestConfig == 161)
			strcpy(itr->second.media,"Video");

		if(itr->second.harvestConfig == 2)
			strcpy(itr->second.media,"Audio");

		if(itr->second.harvestConfig == 516)
			strcpy(itr->second.media,"Subtitle");

	}

	if ((itr->second.bitrate == 0) && (strstr(buffer, "defaultBitrate -") != NULL))
	{
		token = strstr(buffer, "defaultBitrate -");
		itr->second.bitrate = getNumberFromString(token);
	}

	if (strstr(buffer, "harvestCountLimit:") != NULL)
	{
		token = strstr(buffer, "harvestCountLimit:");
		itr->second.harvestFragmentsCount = (int)getNumberFromString(token);
		if (itr->second.harvestFragmentsCount <= 1)
		{
			harvestComplete = true;
		}
	}

	if (strstr(buffer, "Harvest trackId") != NULL)
	{
		token = strstr(buffer, "Harvest trackId");
		itr->second.harvestTrackId = (int)getNumberFromString(token);
	}

	if ((strstr(buffer, "error") != NULL) || (strstr(buffer, "File open failed") != NULL) )
	{
		writeHarvestErrorReport(itr->second,buffer);
			
	}

	if (itr->second.harvestEndFlag == false)
	{
		if ((strstr(buffer, "EndOfStreamReached") != NULL) || harvestComplete == true)
		{
			writeHarvestEndReport(itr->second,buffer);
			itr->second.harvestEndFlag = true;
			return false;  // we are done
		}
	}

	return true;
}

void Harvester::writeHarvestErrorReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{

	FILE *errorfp;
	std::string errorFilename;
	errorFilename = Harvester::mHarvestPath+"/HarvestErrorReport.txt";

	errorfp = fopen(errorFilename.c_str(), "a");

	if (errorfp == NULL)
	{
		printf("Error opening the file %s\n", errorFilename.c_str());
		return;
	}

	if (strstr(buffer, "error") != NULL)
	{
		harvestProfileDetails.harvestErrorCount++;
		fprintf(errorfp, "Error : %s",buffer);
	}

	if (strstr(buffer, "File open failed") != NULL)
	{
		harvestProfileDetails.harvestFailureCount++;
		fprintf(errorfp, "Failed : %s",buffer);
	}

	fclose(errorfp);
}

void Harvester::writeHarvestEndReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{
	FILE *fp;
	std::string filename;
	
	filename = Harvester::mHarvestPath+"/HarvestReport.txt";
	fp = fopen(filename.c_str(), "a");

	if (fp == NULL)
	{
		printf("Error opening the file %s\n", filename.c_str());
		return;
	}
	// harvestFragmentsCount is the number of the last fragment received, counting down from mHarvestCountLimit.
	// So it is 1 if all fragments have been collected. 0 means none received.
	int fragmentCount = harvestProfileDetails.harvestFragmentsCount;
	if (fragmentCount > 0)
	{
		fragmentCount = mHarvester.mHarvestCountLimit - harvestProfileDetails.harvestFragmentsCount +1;
	}
	
	fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n", harvestProfileDetails.media, harvestProfileDetails.bitrate, fragmentCount, harvestProfileDetails.harvestErrorCount, harvestProfileDetails.harvestFailureCount);

	fclose(fp);

}

void Harvester::harvestTerminateHandler(int signal)
{

	if((Harvester::mHarvestReportFlag == true) && (!mHarvestThreadId.empty()))
	{
		FILE *fp;
		std::string filename;

		filename = Harvester::mHarvestPath+"/HarvestReport.txt";
		fp = fopen(filename.c_str(), "a");

		if (fp == NULL)
		{
			printf("Error opening the file %s\n", filename.c_str());
			return;
		}

		for(auto item: mHarvestThreadId)
		{
			auto itr = mHarvestInfo.find(item);

			if((itr != mHarvestInfo.end()) && (itr->second.harvestEndFlag == false))
			{
				fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n", itr->second.media, itr->second.bitrate, mHarvester.mHarvestCountLimit - itr->second.harvestFragmentsCount, itr->second.harvestErrorCount, itr->second.harvestFailureCount);

				itr->second.harvestEndFlag = true;
			}
		}

		fclose(fp);
	}

	printf("Signal handler\n");
	
	exit(signal);
}

FILE * Harvester::createSlaveHarvester(std::map<std::string, std::string> cmdlineParams, int harvestWhat, long bitrate,
									   std::string language, int trackId)
{
	std::string slaveCmd;
	if(Harvester::mExePathName[0] != '\0')
	{
		slaveCmd.append(Harvester::mExePathName);
	}

	slaveCmd = slaveCmd +
		" harvest harvestMode=Slave harvestUrl=" + cmdlineParams["harvestUrl"] +
		" harvestConfig=" + std::to_string(harvestWhat) +
		" abr=false" +
		" useTCPServerSink=true" +
		" TCPServerSinkPort=" + std::to_string(Harvester::mTCPServerSinkPort);
	
	if (mHarvester.mUseTCPServerSink)
	{
		slaveCmd = slaveCmd + " useTCPServerSink=true TCPServerSinkPort=" + std::to_string(Harvester::mTCPServerSinkPort);
		if (mHarvester.mTCPServerSinkPort > 0)
		{
			mHarvester.mTCPServerSinkPort += 2;  // increment for next aamp-cli as already used on master tune
		}
	}
	if (mHarvester.mSuppressDecode)
	{
		slaveCmd = slaveCmd + " suppressDecode=true";
	}
	
	if (mHarvester.mHarvestCountLimit > 0)
	{
		slaveCmd = slaveCmd + " harvestCountLimit=" + std::to_string(mHarvester.mHarvestCountLimit);
	}
	if (mHarvester.mHarvestDuration > 0)
	{
		slaveCmd = slaveCmd + " harvestDuration=" + std::to_string(mHarvester.mHarvestDuration);;
	}

	if(cmdlineParams.find("harvestPath") != cmdlineParams.end())
	{
		slaveCmd = slaveCmd + " harvestPath=" + cmdlineParams["harvestPath"];
	}
	
	if (bitrate != 0)
	{
		slaveCmd = slaveCmd + " defaultBitrate="+std::to_string(bitrate);
		slaveCmd = slaveCmd + " defaultBitrate4k="+std::to_string(bitrate);
	}

	if (language != "")
	{
		if ((harvestWhat & (getHarvestConfigForMedia(eMEDIATYPE_AUDIO))) != 0)
		{
			slaveCmd = slaveCmd + " preferredAudioLanguage="+language;
		}
		else if ((harvestWhat & (getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE))) !=0)
		{
			slaveCmd = slaveCmd + " preferredTextLanguage="+language;
		}
	}

	if (trackId > -1)
	{
		slaveCmd = slaveCmd + " trackId="+std::to_string(trackId);
	}

	printf("Create process of slave harvest command %s \n", slaveCmd.c_str());
	mHarvester.writeHarvestReport((char *)slaveCmd.c_str(), false);
	
	if(cmdlineParams.find("noHarvest") == cmdlineParams.end())
	{
		return popen(slaveCmd.c_str(), "r");
	}
	return NULL;
}

bool Harvester::createSlaveDataReader(FILE *pSlaveHarvester, std::thread& dataReader)
{
	bool failed = false;
	try
	{
		dataReader = std::thread(&Harvester::slaveDataOutput, pSlaveHarvester);
	}
	catch (std::exception& e)
	{
		printf("Error at thread create: slaveDataOutput thread : %s\n", e.what());
		failed = true;
	}

	if (!failed)
	{
		{
			std::lock_guard<std::mutex> guard(mHarvestInfoMutex);
			// Add to thread/results map.
			std::thread::id threadId = dataReader.get_id();
			if(mHarvestInfo.find(threadId) == mHarvestInfo.end())
			{
				HarvestProfileDetails l_harvestProfileDetails;
				memset(&l_harvestProfileDetails,'\0',sizeof(l_harvestProfileDetails));
				
				mHarvestInfo.insert( std::make_pair(threadId, l_harvestProfileDetails) );
				mHarvestThreadId.push_back(threadId);
			}
		}
		printf("Successfully launched harvest data reader.\n" );
		return true;
	}
	return false;
	
}

#ifdef __linux__
void Harvester::getExecutablePath()
{
   realpath(PROC_SELF_EXE, Harvester::mExePathName);
}
#endif

#ifdef __APPLE__
void Harvester::getExecutablePath()
{
	char rawPathName[PATH_MAX];
	uint32_t rawPathSize = (uint32_t)sizeof(rawPathName);

	if(!_NSGetExecutablePath(rawPathName, &rawPathSize))
	{
		realpath(rawPathName, Harvester::mExePathName);
	}
}
#endif 
