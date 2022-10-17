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
 * @file AampcliHarvestor.cpp
 * @brief Stand alone AAMP player with command line interface.
 */

#include <unistd.h>
#include <exception>
#include "AampcliHarvestor.h"
#include "AampcliPlaybackCommand.h"

Harvestor::Harvestor() : mMasterHarvestorThreadID(),
			 mSlaveHarvestorThreadID(),
			 mReportThread(),
			 mSlaveVideoThreads{},
			 mSlaveAudioThreads{},
			 mSlaveSubtitleThreads{},
			 mSlaveIFrameThread()
{}

std::map<std::thread::id, HarvestProfileDetails> Harvestor::mHarvestInfo = std::map<std::thread::id, HarvestProfileDetails>();
std::vector<std::thread::id> Harvestor::mHarvestThreadId(0);
PlayerInstanceAAMP * Harvestor::mPlayerInstanceAamp = NULL;
bool Harvestor::mHarvestReportFlag = false;
std::string Harvestor::mHarvestPath = "";
char Harvestor::mExePathName[PATH_MAX] = "";
int Harvestor::mHarvestCountLimit = 0;
int Harvestor::mTCPServerSinkPort = 0;
Harvestor mHarvestor;

#define HARVEST_CMD_LEN (8) // "harvest "

bool Harvestor::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	char harvestCmd[mHarvestCommandLength] = {'\0'};

	Harvestor::mPlayerInstanceAamp = playerInstanceAamp;

	auto len = strlen(cmd);
	if ((int)len <= HARVEST_CMD_LEN)
	{
		printf("[AAMPCLI] Incomplete harvest command '%s'\n", cmd);
		return true;
	}
	
	strncpy(harvestCmd, cmd+HARVEST_CMD_LEN, len-HARVEST_CMD_LEN);
	
	if( PlaybackCommand::isCommandMatch(harvestCmd,"harvestMode=Master") )
	{
		printf("%s:%d: thread create:MasterHarvestor thread\n", __FUNCTION__, __LINE__);
		try {
			mMasterHarvestorThreadID = std::thread (&Harvestor::masterHarvestor, harvestCmd);
		}
		catch (std::exception& e)
		{
			printf("%s:%d: Error at thread create:MasterHarvestor thread : %s\n", __FUNCTION__, __LINE__, e.what());
		}
	}
	else if( PlaybackCommand::isCommandMatch(harvestCmd,"harvestMode=Slave") )
	{
		printf("%s:%d: thread create:SlaveHarvestor thread\n", __FUNCTION__, __LINE__);
		try {
			mSlaveHarvestorThreadID =  std::thread (&Harvestor::slaveHarvestor, harvestCmd);
		}catch (std::exception& e)
		{
			printf("%s:%d: Error at  thread create:SlaveHarvestor thread : %s\n", __FUNCTION__, __LINE__, e.what());
		}
	}
	else
	{
		printf("[AAMPCLI] Invalid harvest command '%s'\n", cmd);
	}

	if (mMasterHarvestorThreadID.joinable())
	{
		mMasterHarvestorThreadID.join();
	}

	if (mSlaveHarvestorThreadID.joinable())
	{
		mSlaveHarvestorThreadID.join();
	}

	return true;
}

std::vector<AudioTrackInfo> Harvestor::GetAudioTracks()
{
	std::vector<AudioTrackInfo> audioTrackVec;
	std::string json = mHarvestor.mPlayerInstanceAamp->GetAvailableAudioTracks(true);

	cJSON *root = cJSON_Parse(json.c_str());
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
		trackInfo.bandwidth = cJSON_GetObjectItem(arrayItem, "bandwidth")->valuedouble;
		trackInfo.language = cJSON_GetObjectItem(arrayItem, "language")->valuestring;
		audioTrackVec.push_back(trackInfo);
	}

	return audioTrackVec;
}

std::vector<TextTrackInfo> Harvestor::GetTextTracks()
{
	std::vector<TextTrackInfo> textTrackVec;
	std::string json = mHarvestor.mPlayerInstanceAamp->GetAvailableTextTracks(true);

	cJSON *root = cJSON_Parse(json.c_str());
	cJSON *arrayItem = NULL;
	int arraySize = cJSON_GetArraySize(root);
	for ( int item = 0; item < arraySize; item++)
	{
		// As we don't get any sort of index, we'll just try to create a unique value for the item
		arrayItem = cJSON_GetArrayItem(root, item);
		std::string arrayItemText = cJSON_Print(arrayItem);
		std::size_t itemHash = std::hash<std::string>{}(arrayItemText);

		// Just fill the fields we require
		TextTrackInfo trackInfo;
		trackInfo.index = std::to_string(itemHash);
		trackInfo.language = cJSON_GetObjectItem(arrayItem, "language")->valuestring;
		textTrackVec.push_back(trackInfo);
	}

	return textTrackVec;
}


void Harvestor::masterHarvestor(void * arg)
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

	if(aamp_pthread_setname(pthread_self(), "MasterHarvestor"))
	{
		printf("%s:%d: aamp_pthread_setname failed\n",__FUNCTION__,__LINE__);
	}

	mHarvestor.getExecutablePath();
	
	while(std::getline(ss, item, ' ')) {
		params.push_back(item);
	}
	params.push_back("harvestConfig=65535");
	params.push_back("defaultBitrate=9999999");
	params.push_back("defaultBitrate4K=9999999");
	params.push_back("abr=false");
	params.push_back("useTCPServerSink=true");

	for(std::string param : params)
	{
		std::size_t delimiterPos = param.find("=");

		if((delimiterPos != std::string::npos) && (param.substr(delimiterPos + 1) != ""))
		{
			mHarvestor.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
			mHarvestor.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
		else
		{
			printf("%s:%d: Error in command param %s\n",__FUNCTION__,__LINE__,param.c_str());
			return;
		}
	}

	// Make sure we have a valid harvest count limit; priority: set by harvest command option, set by aamp.cfg, default if not set
	mHarvestor.mHarvestCountLimit = mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestCountLimit);
	if (mHarvestor.mHarvestCountLimit == 0)
	{
		mHarvestor.mHarvestCountLimit = 9999999;
	}
	mHarvestor.mTCPServerSinkPort = mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_TCPServerSinkPort);
	mHarvestor.mTCPServerSinkPort += 2;  // increment for next aamp-cli as already used on master tune
	
	if(cmdlineParams.find("harvestUrl") != cmdlineParams.end())
	{
		mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

		Harvestor::mHarvestPath = cmdlineParams["harvestPath"];

		try
		{
			mHarvestor.startHarvestReport((char *) cmdlineParams["harvestUrl"].c_str());
		}
		catch (std::exception& e)
		{
			printf("%s:%d: Error at startHarvestReport : %s\n",__FUNCTION__,__LINE__, e.what());
		}
		
		pIframe = mHarvestor.createSlaveHarvestor (cmdlineParams, getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO));
		if (pIframe != NULL)
		{
			mHarvestor.createSlaveDataReader(pIframe, mHarvestor.mSlaveIFrameThread);
		}
		else
		{
			printf("%s:%d: could not create slave harvest process for iframe.", __FUNCTION__, __LINE__);
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

			if(mHarvestor.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
			{
				sleep(5); //Waiting for slave process to complete
			}

			if ((mHarvestor.mPlayerInstanceAamp->GetState() != eSTATE_COMPLETE) && (mHarvestor.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING))
			{
				printf("%s:%d: Tune stopped\n",__FUNCTION__,__LINE__);
				mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());
			}

			tempVideoBitrates = mHarvestor.mPlayerInstanceAamp->GetVideoBitrates();
			std::sort(tempVideoBitrates.begin(), tempVideoBitrates.end());

			currentAudioTrackInfo = mHarvestor.GetAudioTracks();
			currentTextTrackInfo = mHarvestor.GetTextTracks();
						
			std::vector<long>::iterator position = std::find(tempVideoBitrates.begin(), tempVideoBitrates.end(), mHarvestor.mPlayerInstanceAamp->GetVideoBitrate());
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

			if(diffVideoBitrates.size())
			{
				for(auto bitrate:diffVideoBitrates)
				{
					FILE *p = NULL;
					
					p = mHarvestor.createSlaveHarvestor (cmdlineParams, getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_LICENCE), bitrate);
					if (p != NULL)
					{
						if (mHarvestor.createSlaveDataReader(p, mHarvestor.mSlaveVideoThreads[videoThreadId]) == true)
						{
							videoThreadId++;
						}
					}
					else
					{
						printf("%s:%d: could not create slave harvest process for video.", __FUNCTION__, __LINE__);
					}
				}
				cacheVideoBitrates = tempVideoBitrates;
			}

			if(diffAudioTrackInfo.size())
			{
				for(int trackId = 0; trackId < diffAudioTrackInfo.size(); trackId++)
				{
					FILE *p = NULL;
					
					p = mHarvestor.createSlaveHarvestor (cmdlineParams, getHarvestConfigForMedia(eMEDIATYPE_AUDIO), 
					                                     diffAudioTrackInfo[trackId].bandwidth,
														 diffAudioTrackInfo[trackId].language, trackId);
					if (p != NULL)
					{
						if (mHarvestor.createSlaveDataReader(p, mHarvestor.mSlaveAudioThreads[audioThreadId]) == true)
						{
							audioThreadId++;
						}
					}
					else
					{
						printf("%s:%d: could not create slave harvest process for audio.", __FUNCTION__, __LINE__);
					}
				}

				cacheAudioTrackInfo = currentAudioTrackInfo;
			}

			if(diffTextTrackInfo.size())
			{
				for(int trackId = 0; trackId < diffTextTrackInfo.size(); trackId++)
				{
					FILE *p = NULL;
					
					p = mHarvestor.createSlaveHarvestor (cmdlineParams, 
					                                     getHarvestConfigForMedia(eMEDIATYPE_INIT_SUBTITLE)|getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE), 
					                                     0L,
														 diffTextTrackInfo[trackId].language, trackId);
					if (p != NULL)
					{
						if (mHarvestor.createSlaveDataReader(p, mHarvestor.mSlaveSubtitleThreads[subtitleThreadId]) == true)
						{
							subtitleThreadId++;
						}
					}
					else
					{
						printf("%s:%d: could not create slave harvest process for subtitle.", __FUNCTION__, __LINE__);
					}
				}

				cacheTextTrackInfo = currentTextTrackInfo;
			}

			// We are done when all of the data readers are done
			bool done = true;
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

		if (mHarvestor.mSlaveIFrameThread.joinable())
		{
			mHarvestor.mSlaveIFrameThread.join();
		}

		for(int i = 0; i < videoThreadId; i++)
		{
			if (mHarvestor.mSlaveVideoThreads[i].joinable())
			{
				mHarvestor.mSlaveVideoThreads[i].join();
			}
		}

		for(int i =0; i < audioThreadId; i++)
		{
			if(mHarvestor.mSlaveAudioThreads[i].joinable())
			{
				mHarvestor.mSlaveAudioThreads[i].join();
			}
		}

		for(int i =0; i < subtitleThreadId; i++)
		{
			if(mHarvestor.mSlaveSubtitleThreads[i].joinable())
			{
    			mHarvestor.mSlaveSubtitleThreads[i].join();
            }
		}

	}
	else
	{
		printf("%s:%d: harvestUrl not found\n",__FUNCTION__,__LINE__);
	}

	return;
}

void Harvestor::slaveHarvestor(void * arg)
{
	char * cmd = (char *)arg;
	std::map<std::string, std::string> cmdlineParams;
	std::vector<std::string> params;
	std::stringstream ss(cmd);
	std::string item,harvestUrl;

	if(aamp_pthread_setname(pthread_self(), "SlaveHarvestor"))
	{
		printf("%s:%d: SlaveHarvestor_thread aamp_pthread_setname failed\n",__FUNCTION__,__LINE__);
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
			mHarvestor.mPlayerInstanceAamp->mConfig.ProcessConfigText(param, AAMP_DEV_CFG_SETTING);
			mHarvestor.mPlayerInstanceAamp->mConfig.DoCustomSetting(AAMP_DEV_CFG_SETTING);
			cmdlineParams[ param.substr(0, delimiterPos) ] = param.substr(delimiterPos + 1);
		}
		else
		{
			printf("%s:%d: Error in command param %s\n",__FUNCTION__,__LINE__,param.c_str());
			return;
		}

	}


	int harvestConfig = mHarvestor.mPlayerInstanceAamp->mConfig.GetConfigValue(eAAMPConfig_HarvestConfig) ;

	mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

	if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)))
	{
		float rate = 2;
		mHarvestor.mPlayerInstanceAamp->SetRate(rate);

	}
	else if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_AUDIO)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvestor.mPlayerInstanceAamp->SetAudioTrack(trackId);	
		printf("%s:%d: Harvest audio trackId %d\n",__FUNCTION__,__LINE__,trackId);
	}
	else if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_INIT_SUBTITLE)|getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE)))
	{
		int trackId = stoi(cmdlineParams["trackId"]);
		sleep(5);
		mHarvestor.mPlayerInstanceAamp->SetTextTrack(trackId);
		printf("%s:%d: Harvest subtitle trackId %d\n",__FUNCTION__,__LINE__,trackId);
	}

	while (1)
	{
		sleep(1);

		if(mHarvestor.mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("%s:%d: Tune completed exiting harvesting mode\n",__FUNCTION__,__LINE__);
			break;
		}

		if (mHarvestor.mPlayerInstanceAamp->GetState() > eSTATE_PLAYING)
		{
			mHarvestor.mPlayerInstanceAamp->Tune(cmdlineParams["harvestUrl"].c_str());

			if(harvestConfig == (getHarvestConfigForMedia(eMEDIATYPE_VIDEO)|getHarvestConfigForMedia(eMEDIATYPE_IFRAME)|getHarvestConfigForMedia(eMEDIATYPE_INIT_VIDEO)))
			{
				float rate = 2;
				mHarvestor.mPlayerInstanceAamp->SetRate(rate);

			}
		}
		
		int currHarvestCount = mHarvestor.mPlayerInstanceAamp->aamp->GetHarvestRemainingFragmentCount();
		if (currHarvestCount == 0)
		{
			printf("%s:%d: Harvest count completed, exiting harvesting mode\n",__FUNCTION__,__LINE__);
			sleep(5);  // allow logs to propogate to slaveDataOutput
			break;
		}
	}
	exit(0);
	return;
}

void Harvestor::slaveDataOutput(void * arg)
{
	FILE * pipe;
	pipe = (FILE *) arg;
	char buffer[500] = {'\0'};
	bool active = false;
	if(aamp_pthread_setname(pthread_self(), "SlaveOutput"))
	{
		printf("%s:%d: aamp_pthread_setname failed\n",__FUNCTION__, __LINE__);
	}

	while (!feof(pipe))
	{
		if (fgets(buffer, 500, pipe) != NULL)
		{
			active = mHarvestor.getHarvestReportDetails(buffer);
			printf("%s:%d: [0x%p] %s",__FUNCTION__, __LINE__,pipe,buffer);
		}
		
		if (active == false) // end this thread
		{
			printf("%s:%d: harvesting complete, exiting thread.\n",__FUNCTION__, __LINE__);
			break;
		}
	}

	return;
}

long Harvestor::getNumberFromString(std::string buffer)
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

void Harvestor::startHarvestReport(char * harvesturl)
{
	FILE *fp;
	FILE *errorfp;
	std::string filename;
	std::string errorFilename;

	filename = Harvestor::mHarvestPath+"/HarvestReport.txt";
	errorFilename = Harvestor::mHarvestPath+"/HarvestErrorReport.txt";

	fp = fopen(filename.c_str(), "w");

	if (fp == NULL)
	{
		printf("Error opening the file %s\n", filename.c_str());
		return;
	}

	fprintf(fp, "Harvest Profile Details\n\n");
	fprintf(fp, "Harvest url : %s\n",harvesturl);
	fprintf(fp, "Harvest mode : Master\n");
	fclose(fp);

	Harvestor::mHarvestReportFlag = true;

	errorfp = fopen(errorFilename.c_str(), "w");

	if (errorfp == NULL)
	{
		printf("Error opening the file %s\n", errorFilename.c_str());
		return;
	}

	fprintf(errorfp, "Harvest Error Report\n\n");
	fprintf(errorfp, "Harvest url : %s\n",harvesturl);
	fclose(errorfp);
	return;
}

bool Harvestor::getHarvestReportDetails(char *buffer)
{
	char *token = NULL;
	std::thread::id threadId;
	HarvestProfileDetails l_harvestProfileDetails;
	std::map<std::thread::id, HarvestProfileDetails>::iterator itr;
	bool harvestComplete = false;

	memset(&l_harvestProfileDetails,'\0',sizeof(l_harvestProfileDetails));
	threadId = std::this_thread::get_id();

	if(mHarvestInfo.find(threadId) == mHarvestInfo.end())
	{
		mHarvestInfo.insert( std::make_pair(threadId, l_harvestProfileDetails) );
		mHarvestThreadId.push_back(threadId);
	}

	itr = mHarvestInfo.find(threadId);

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

void Harvestor::writeHarvestErrorReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{

	FILE *errorfp;
	std::string errorFilename;
	errorFilename = Harvestor::mHarvestPath+"/HarvestErrorReport.txt";

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

void Harvestor::writeHarvestEndReport(HarvestProfileDetails harvestProfileDetails, char *buffer)
{
	FILE *fp;
	std::string filename;
	
	filename = Harvestor::mHarvestPath+"/HarvestReport.txt";
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
		fragmentCount = mHarvestor.mHarvestCountLimit - harvestProfileDetails.harvestFragmentsCount +1;
	}
	
	fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n", harvestProfileDetails.media, harvestProfileDetails.bitrate, fragmentCount, harvestProfileDetails.harvestErrorCount, harvestProfileDetails.harvestFailureCount);

	fclose(fp);

}

void Harvestor::harvestTerminateHandler(int signal)
{

	if((Harvestor::mHarvestReportFlag == true) && (!mHarvestThreadId.empty()))
	{
		FILE *fp;
		std::string filename;
		//int harvestCountLimit = 0;

		filename = Harvestor::mHarvestPath+"/HarvestReport.txt";
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
				// harvestFragmentsCount is the number of the last fragment received, counting down from mHarvestCountLimit.
				// So it is 1 if all fragments have been collected. 0 means none received.
				int fragmentCount = itr->second.harvestFragmentsCount;
				if (fragmentCount > 0)
				{
					fragmentCount = mHarvestor.mHarvestCountLimit - itr->second.harvestFragmentsCount +1;
				}
				
				fprintf(fp, "\nMedia : %s\nBitrate : %lu\nTotal fragments count : %d\nError fragments count : %d\nFailure fragments count : %d\n", itr->second.media, itr->second.bitrate, mHarvestor.mHarvestCountLimit - itr->second.harvestFragmentsCount, itr->second.harvestErrorCount, itr->second.harvestFailureCount);

				itr->second.harvestEndFlag = true;
			}
		}

		fclose(fp);
	}

	printf("%s:%d: Signal handler\n",__FUNCTION__, __LINE__);
	
	exit(signal);
}

FILE * Harvestor::createSlaveHarvestor(std::map<std::string, std::string> cmdlineParams, int harvestWhat, long bitrate, 
									   std::string language, int trackId)
{
	std::string slaveCmd;
	if(Harvestor::mExePathName[0] != '\0')
	{
		slaveCmd.append(Harvestor::mExePathName);
	}

	slaveCmd = slaveCmd +
		" harvest harvestMode=Slave harvestUrl=" + cmdlineParams["harvestUrl"] +
		" harvestCountLimit=" + std::to_string(Harvestor::mHarvestCountLimit) +
		" harvestConfig=" + std::to_string(harvestWhat) +
		" abr=false" +
		" useTCPServerSink=true" +
		" TCPServerSinkPort=" + std::to_string(Harvestor::mTCPServerSinkPort);
	
	Harvestor::mTCPServerSinkPort += 2;

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
		if(harvestWhat == (getHarvestConfigForMedia(eMEDIATYPE_AUDIO)))
		{
			slaveCmd = slaveCmd + " preferredAudioLanguage="+language;
		}
		else if(harvestWhat == (getHarvestConfigForMedia(eMEDIATYPE_INIT_SUBTITLE)|getHarvestConfigForMedia(eMEDIATYPE_SUBTITLE)))
		{
			slaveCmd = slaveCmd + " preferredTextLanguage="+language;
		}
	}

	if (trackId > -1)
	{
		slaveCmd = slaveCmd + " trackId="+std::to_string(trackId);
	}

	printf("%s:%d: Create process of slave harvest command %s \n", __FUNCTION__, __LINE__, slaveCmd.c_str());
	return popen(slaveCmd.c_str(), "r");
}

bool Harvestor::createSlaveDataReader(FILE *pSlaveHarvestor, std::thread& dataReader)
{
	bool failed = false;
	try
	{
		dataReader = std::thread(&Harvestor::slaveDataOutput, pSlaveHarvestor);
	}
	catch (std::exception& e)
	{
		printf("%s:%d: Error at thread create: slaveDataOutput thread : %s\n",__FUNCTION__,__LINE__, e.what());
		failed = true;
	}

	if (!failed)
	{
		printf("%s:%d: Sucessfully launched harvest for IFrame\n",__FUNCTION__,__LINE__);
		return true;
	}
	return false;
	
}

#ifdef __linux__
void Harvestor::getExecutablePath()
{
   realpath(PROC_SELF_EXE, Harvestor::mExePathName);
}
#endif

#ifdef __APPLE__
void Harvestor::getExecutablePath()
{
	char rawPathName[PATH_MAX];
	uint32_t rawPathSize = (uint32_t)sizeof(rawPathName);

	if(!_NSGetExecutablePath(rawPathName, &rawPathSize))
	{
		realpath(rawPathName, Harvestor::mExePathName);
	}
}
#endif 
