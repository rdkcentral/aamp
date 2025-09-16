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
 * @file AampcliPlaybackCommand.cpp
 * @brief Aampcli Playback Commands handler
 */

#include <iomanip>
#include <regex>
#include "Aampcli.h"
#include "AampcliPlaybackCommand.h"
#include "scte35/AampSCTE35.h"
#include "AampStreamSinkManager.h"
#include <curl/curl.h>

extern VirtualChannelMap mVirtualChannelMap;
extern Aampcli mAampcli;

std::map<std::string,std::string> PlaybackCommand::playbackCommands = std::map<std::string,std::string>();
std::vector<std::string> PlaybackCommand::commands(0);
static std::string mFogHostPrefix="127.0.0.1:9080"; //Default host string for "fog" command
std::vector<AdvertInfo> mAdvertList;

void PlaybackCommand::getRange(const char* cmd, unsigned long& start, unsigned long& end, unsigned long& tail)
{
	//Parse the command line to see if all lines should be displayed, a range from start to end, or a number of lines from the end of the list.
	//If tail is 0, start & end specify the range. If tail is non-zero it is the number of lines to display from the end of the list.
	start = 0;
	end = ULLONG_MAX;
	tail = 0;
	if( !strcmp(cmd, "list"))
	{
		// just "list", no parameters.
	}
	else
	if( sscanf(cmd, "list %lu-%lu", &start, &end) == 2 )
	{
		if(start != 0)
		{
			start--;
			end--;
		}
		//A range of lines from the list with start & end
	}
	else
	if( sscanf(cmd, "list -%lu", &tail) != 1 )
	{
		//Check for "list -n". tail is the number of lines to display at the end of the list.
		if( sscanf(cmd, "list %lu", &start) == 1 )
		{
			if(start != 0)
			{
				start--;
			}
			//Display lines from the specified start to the end of the list.
		}
	}
	if(start > end)
	{
		start = 0;
		end = ULLONG_MAX;
		tail = 0;
	}
}

/**
 * @brief Take a URL of playable content & convert it to a URL which plays via fog.
 * @param host A host string where fog is running, e.g. "127.0.0.1:9080".
 * @param url A URL which could be used on its own to play content without using fog.
 * @param fogUrl The newly created "fog" url
 */
static void buildFogUrl(const std::string host, const char* url, std::string& fogUrl)
{
	fogUrl = "http://" + host + "/tsb?clientId=FOG_AAMP&recordedUrl=";
	std::string inStr(url);
	std::string outStr;
	UrlEncode(inStr, outStr);
	fogUrl += outStr;
}

PlayerInstanceAAMP * PlaybackCommand::findPlayerInstance( const char *playerRef )
{
	if( playerRef[0] )
	{
		if(isNumber(playerRef))
		{
			int playerId = atoi(playerRef);
			for( auto player : mAampcli.mPlayerInstances )
			{
				if( player->GetId() == playerId )
				{
					return player;
				}
			}
		}
		else
		{
			for( auto player : mAampcli.mPlayerInstances )
			{
				if( player->GetAppName() == playerRef )
				{
					return player;
				}
			}
		}
	}
	return NULL;
}

void PlaybackCommand::HandleCommandList( const char *cmd )
{
	unsigned long start, end, tail;
	getRange(cmd, start, end, tail);
	mVirtualChannelMap.showList(start, end, tail);
}

void PlaybackCommand::HandleCommandBatch()
{
	std::string batchFilePath = std::string(getenv("HOME")) + "/aampcli.bat";
	std::ifstream batchFile(batchFilePath);

	if (!batchFile.is_open())
	{
		AAMPCLI_PRINTF("[AAMPCLI] ERROR - unable to open batch file\n");
		return;
	}

	std::string line;
	while (std::getline(batchFile, line))
	{
		if (line.compare("batch") == 0)
		{
			AAMPCLI_PRINTF("[AAMPCLI] ERROR - nested batch command not allowed\n");
			continue;
		}
		
		else if (!line.empty() && line[0] != '#')
		{
			AAMPCLI_PRINTF("[AAMPCLI] Executing command: %s\n", line.c_str());
			execute(line.c_str(), mAampcli.mSingleton);
		}
	}
}


void PlaybackCommand::HandleCommandContentType( const char *cmd )
{
	if (strlen(cmd) > strlen("contentType "))
	{
		mAampcli.mContentType = std::string(&cmd[strlen("contentType ")]);
		AAMPCLI_PRINTF("[AAMPCLI] contentType set to %s\n", mAampcli.mContentType.c_str());
	}
	else
	{
		mAampcli.mContentType.clear();
		AAMPCLI_PRINTF("[AAMPCLI] contentType not set\n");
	}
}

void PlaybackCommand::HandleCommandNew( const char *cmd )
{
	char playerName[50] = {'\0'};
	if (sscanf(cmd, "new %49s", playerName) == 1)
	{
		mAampcli.newPlayerInstance(playerName);
	}
	else
	{
		mAampcli.newPlayerInstance();
	}
}

void PlaybackCommand::HandleCommandSelect( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	char playerRef[50] = {'\0'};
	if( sscanf(cmd, "select %49s", playerRef ) == 1 )
	{
		PlayerInstanceAAMP *found = findPlayerInstance(playerRef);
		if( found )
		{
			//if( found->aamp )
			{ //Do not edit or remove this following AAMPCLI_PRINTF - it is used in L2 test
				AAMPCLI_PRINTF( "selected player %d (at %p) %s\n",
					   found->GetId(),
					   found,
					   found->GetAppName().c_str() );

				mAampcli.mSingleton = found;
			}
			//else
			///{
			//	AAMPCLI_PRINTF( "error - player exists but is not valid/ready, playerInstanceAamp->aamp is not a valid ptr\n");
			//}
		}
		else
		{
			AAMPCLI_PRINTF( "No player with ID or name '%s'\n", playerRef);
		}
	}
	else
	{
		AAMPCLI_PRINTF( "player instances:\n" );
		for( auto player : mAampcli.mPlayerInstances )
		{ // list available player instances
			AAMPCLI_PRINTF( "\t%d %s", player->GetId(), player->GetAppName().c_str() );
			if( player == playerInstanceAamp )
			{
				AAMPCLI_PRINTF( " (selected)");
			}
			AAMPCLI_PRINTF( "\n");
		}
	}
}

void PlaybackCommand::HandleCommandRelease( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	char playerRef[50] = {'\0'};
	if( sscanf(cmd, "release %49s", playerRef ) == 1 )
	{
		PlayerInstanceAAMP *found = findPlayerInstance(playerRef);
		if( found )
		{
			if( found == playerInstanceAamp)
			{
				AAMPCLI_PRINTF( "Can not release the active player.\n");
			}
			else
			{
				auto it = std::find(mAampcli.mPlayerInstances.begin(), mAampcli.mPlayerInstances.end(), found );
				if (it != mAampcli.mPlayerInstances.end())
				{
					mAampcli.mPlayerInstances.erase(it);
				}
				found->UnRegisterEvents(mAampcli.mEventListener);
				delete(found);
			}
		}
	}
}

void PlaybackCommand::HandleCommandSleep( const char *cmd )
{
	int ms = 0;
	if( sscanf(cmd, "sleep %d", &ms ) == 1 && ms>0 )
	{
		AAMPCLI_PRINTF( "sleeping for %f seconds\n", ms/1000.0 );
		g_usleep (ms * 1000);
		//Do not edit or remove this following AAMPCLI_PRINTF - it is used in L2 test
		AAMPCLI_PRINTF( "sleep complete\n" );
	}
}

void PlaybackCommand::HandleCommandTuneLocator( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	const auto sid = mAampcli.GetSessionId();
	const char *contentType = (mAampcli.mContentType.empty()) ? nullptr : mAampcli.mContentType.c_str();
	if (sid.empty())
	{
		playerInstanceAamp->Tune(cmd, mAampcli.mbAutoPlay, contentType);
	}
	else
	{
		playerInstanceAamp->Tune(cmd, mAampcli.mbAutoPlay,
			contentType, true, false, nullptr, true, nullptr, 0,
			std::move(sid),nullptr);
	}
}

void PlaybackCommand::HandleCommandNext( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	VirtualChannelInfo *pNextChannel = mVirtualChannelMap.next();
	if (pNextChannel)
	{
		AAMPCLI_PRINTF("[AAMPCLI] next %d: %s\n", pNextChannel->channelNumber, pNextChannel->name.c_str());
		mVirtualChannelMap.tuneToChannel( *pNextChannel, playerInstanceAamp, mAampcli.mbAutoPlay );
	}
	else
	{
		AAMPCLI_PRINTF("[AAMPCLI] can not fetch 'next' channel, empty virtual channel map\n");
	}
}

void PlaybackCommand::HandleCommandPrev( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	VirtualChannelInfo *pPrevChannel = mVirtualChannelMap.prev();
	if (pPrevChannel)
	{
		AAMPCLI_PRINTF("[AAMPCLI] next %d: %s\n", pPrevChannel->channelNumber, pPrevChannel->name.c_str());
		mVirtualChannelMap.tuneToChannel( *pPrevChannel, playerInstanceAamp, mAampcli.mbAutoPlay );
	}
	else
	{
		AAMPCLI_PRINTF("[AAMPCLI] can not fetch 'prev' channel, empty virtual channel map\n");
	}
}

void PlaybackCommand::HandleCommandTuneIndex( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	int channelNumber = atoi(cmd);  // invalid input results in 0 -- will not be found

	VirtualChannelInfo *pChannelInfo = mVirtualChannelMap.find(channelNumber);
	if (pChannelInfo != NULL)
	{
		AAMPCLI_PRINTF("[AAMPCLI] channel number: %d\n", channelNumber);
		mVirtualChannelMap.tuneToChannel( *pChannelInfo, playerInstanceAamp, mAampcli.mbAutoPlay );
	}
	else
	{
		AAMPCLI_PRINTF("[AAMPCLI] channel number: %d was not found\n", channelNumber);
	}
}

void PlaybackCommand::HandleCommandSetConfig( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{ //look for first char of json
	const char *json = strchr(cmd,'{');
	if (json)
	{
		if( playerInstanceAamp->InitAAMPConfig(json) )
		{
			return;
		}
	}
	AAMPCLI_PRINTF("Invalid json, note the use of dbl quotes. E.G\nsetconfig {\"info\":true}\n");
}

void PlaybackCommand::HandleCommandGetConfig( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	std::string cfgstring=playerInstanceAamp->GetAAMPConfig();
	if(!cfgstring.empty())
	{
		AAMPCLI_PRINTF("config:  \n%s\n",cfgstring.c_str());
	}
	else
	{
		AAMPCLI_PRINTF("Error : Config is Empty ");
	}
}

void PlaybackCommand::HandleCommandExit( void )
{
	for( auto player: mAampcli.mPlayerInstances )
	{
		SAFE_DELETE( player );
	}
	termPlayerLoop();
}

void PlaybackCommand::HandleCommandCustomHeader( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	std::string headerName;
	std::vector<std::string> headerValue;
	char* cmdptr;
	int parameter=0;
	bool isLicenceHeader=false;
	cmdptr = strtok (const_cast<char*>(cmd)," ,");

	AAMPCLI_PRINTF("[AAMPCLI] customheader Command is %s\n" , cmd);

	std::string subString;

	// accepts just one headervalue for now, not an array.
	// header value of '.' -> remove the header, else it adds it
	while (cmdptr)
	{
		subString.assign(cmdptr);
		AAMPCLI_PRINTF("parameter %d, value %.80s\n", parameter, subString.c_str());
		if ( 3==parameter )
		{
			if ((0 == subString.compare(0,4,"true") ))
			{
				AAMPCLI_PRINTF("isLicenceHeader=true\n");
				isLicenceHeader=true;
			}
			else
			{
				AAMPCLI_PRINTF("isLicenceHeader=false\n");
				isLicenceHeader=false;
			}
		}
		else if ( 1==parameter )
		{
			headerName.assign(subString);
			AAMPCLI_PRINTF("headerName=%.80s\n", headerName.c_str());
		}
		else if ( 2==parameter )
		{
			// pass "." to as value to remove the header instead
			if (0==subString.compare("."))
			{
				AAMPCLI_PRINTF("headerValue empty. Header to be removed.\n");
			}
			else
			{
				headerValue.push_back(subString);
				AAMPCLI_PRINTF("headerValue=%.80s\n", headerValue.back().c_str());
			}
		}
		parameter++;
		cmdptr = strtok (NULL, " ,");
	}
	AAMPCLI_PRINTF("isLicenceHeader=%d\n", isLicenceHeader);
	playerInstanceAamp->AddCustomHTTPHeader(headerName, headerValue, isLicenceHeader);
}

void PlaybackCommand::HandleCommandSubtec( void )
{
#define MAX_SCRIPT_PATH_LEN 512
#define MAX_SUBTEC_PATH_LEN 560
	char scriptPath[MAX_SCRIPT_PATH_LEN] = "";
	char subtecCommand[MAX_SUBTEC_PATH_LEN] = "";

	mAampcli.mSingleton->SetCCStatus(true);

	if (mAampcli.getApplicationDir(scriptPath, MAX_SCRIPT_PATH_LEN) > 0)
	{
#ifdef __APPLE__
		snprintf( subtecCommand, MAX_SUBTEC_PATH_LEN, "bash %s/aampcli-run-subtec.sh&\n", scriptPath);
		system(subtecCommand);
#elif __linux__
		snprintf( subtecCommand, MAX_SUBTEC_PATH_LEN, "gnome-terminal -- bash %s/aampcli-run-subtec.sh\n", scriptPath);
		system(subtecCommand);
#else
		AAMPCLI_PRINTF("[AAMPCLI] WARNING - subtec command not supported on platform\n");
#endif
	}
	else
	{
		AAMPCLI_PRINTF("[AAMPCLI] ERROR - unable to get path to subtec run script\n");
	}
}

void PlaybackCommand::HandleCommandUnlock( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	bool eventChange = false;
	long unlockSeconds = 0;
	long grace = 0;
	long time = -1;
	if( sscanf(cmd, "unlock %ld", &unlockSeconds) >= 1 )
	{
		AAMPCLI_PRINTF("[AAMPCLI] unlocking for %ld seconds\n" , unlockSeconds);
		if(-1 == unlockSeconds)
		{
			grace = -1;
		}
		else
		{
			time = unlockSeconds;
		}
	}
	else
	{
		AAMPCLI_PRINTF("[AAMPCLI] unlocking till next program change\n");
		eventChange = true;
	}
	playerInstanceAamp->DisableContentRestrictions(grace, time, eventChange);
}

void PlaybackCommand::HandleCommandAuto( const char *cmd )
{
	int start=1, end=9999; // default range of virtual channels to tune (from aampcli.csv)
	int maxTuneTimeS = 6; // how long to wait before considering a tune failed (important, as some tunes do not promptly surface errors)
	int playTimeS = 15; // how long to leave playing before stopping
	int betweenTimeS = 11; // delay between stop and next tune long enough for automatic cache flush
	(void)sscanf(cmd, "auto %d %d %d %d %d", &start, &end, &maxTuneTimeS, &playTimeS, &betweenTimeS );
	mAampcli.doAutomation( start, end, maxTuneTimeS, playTimeS, betweenTimeS );
}

void PlaybackCommand::HandleCommandFog( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	if(!strcmp(cmd, "fog"))
	{
		AAMPCLI_PRINTF("host: %s\n", mFogHostPrefix.c_str());
	}
	else if(!strncmp(cmd, "fog host=", 9))
	{
		std::string tmpIpv4 = &cmd[9];
		//Match ip address:port. 0 to 255 followed by . {3} times then 0 to 255 then : then number
		std::regex ipv4("(([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]):[0-9]+");
		if (std::regex_match(tmpIpv4, ipv4))
		{
			mFogHostPrefix = tmpIpv4;
			AAMPCLI_PRINTF("host: %s\n", mFogHostPrefix.c_str());
		}
		else
		{
			AAMPCLI_PRINTF("Invalid ip:port\n");
		}
	}
	else
	{
		//Should be a cmd of "fog url". Create fogified URL & try & tune to that.
		if((strlen(cmd) > 4) && aamp_isTuneScheme(&cmd[4]))
		{
			std::string fogUrl;
			buildFogUrl(mFogHostPrefix, &cmd[4], fogUrl);
			AAMPCLI_PRINTF("Tune to: %s\n", fogUrl.c_str());
			playerInstanceAamp->Tune(fogUrl.c_str(), mAampcli.mbAutoPlay);
		}
		else
		{
			AAMPCLI_PRINTF("Invalid URL\n");
		}
	}
}

void PlaybackCommand::HandleCommandAdvert( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	std::istringstream input;
	input.str(cmd);

	std::string token;
	std::getline(input, token, ' ');
	assert(token == "advert");

	if (std::getline(input, token, ' '))
	{
		if( token == "list" )
		{
			AAMPCLI_PRINTF("[AAMP-CLI] Ad Map:\n");
			for( const AdvertInfo &advertInfo : mAdvertList )
			{
				AAMPCLI_PRINTF("[AAMP-CLI] advert %s -> %s\n", advertInfo.adBreakId.c_str(), advertInfo.url.c_str() );
			}
		}
		else if( token == "clear" )
		{
			mAdvertList.clear();
			AAMPCLI_PRINTF("[AAMP-CLI] Cleared Ad List\n");
		}
		else if( token == "map" )
		{
			AdvertInfo advertInfo;
			std::getline( input, advertInfo.adBreakId, ' ' );
			std::getline( input, advertInfo.url, ' ' );
			mAdvertList.push_back(advertInfo);
			AAMPCLI_PRINTF("[AAMP-CLI] mapped adBreakId %s\n", advertInfo.adBreakId.c_str() );
		}
	}
	else
	{
		AAMPCLI_PRINTF("[AAMP-CLI] ERROR - expected 'advert [list, add, rm]'\n");
	}
}

void PlaybackCommand::HandleCommandScte35( const char *cmd )
{
	std::istringstream input;
	input.str(cmd);

	std::string token;
	std::getline(input, token, ' ');
	assert(token == "scte35");

	if (std::getline(input, token, ' '))
	{
		SCTE35SpliceInfo spliceInfo(token);
		AAMPCLI_PRINTF("%s\n", spliceInfo.getJsonString(true).c_str());
	}
	else
	{
		AAMPCLI_PRINTF("[AAMP-CLI] ERROR - expected 'scte35 <base64>'\n");
	}
}

void PlaybackCommand::HandleCommandSessionId( const char *cmd )
{
	char sid[128] = {'\0'};

	AAMPCLI_PRINTF("[AAMPCLI] Matched Command SessionID - %s\n", cmd);
	const auto res = sscanf(cmd, "sessionid %127s", sid);

	if (res == 1)
	{
		mAampcli.SetSessionId({sid});
	}
	else
	{
		for (const auto & inst : mAampcli.mPlayerInstances)
		{
			if (inst)
			{
				const auto index = inst->GetId();
				AAMPCLI_PRINTF("[AAMPCLI] Player: %d - %s | %s\n", index,
					   mAampcli.GetSessionId(index).c_str(), inst->GetSessionId().c_str());
			}
		}
	}
}

void PlaybackCommand::HandleCommandSeek( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	int keepPaused = 0;
	double seconds = 0;
	if (sscanf(cmd, "seek %lf %d", &seconds, &keepPaused) >= 1)
	{
		bool seekWhilePaused = (keepPaused==1);
		playerInstanceAamp->Seek(seconds, seekWhilePaused );
	}
}

void PlaybackCommand::HandleCommandFF( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	int rate;
	if (sscanf(cmd, "ff%d", &rate) == 1)
	{
		playerInstanceAamp->SetRate(rate);
	}
}
void PlaybackCommand::HandleCommandREW( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	int rate;
	if (sscanf(cmd, "rew%d", &rate) == 1)
	{
		playerInstanceAamp->SetRate(-rate);
	}
}

void PlaybackCommand::HandleCommandPause(const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	double seconds = 0.0;
	if (sscanf(cmd, "pause %lf", &seconds) == 1)
	{
		playerInstanceAamp->PauseAt(seconds);
	}
	else
	{
		playerInstanceAamp->SetRate(0);
	}
}

void PlaybackCommand::HandleCommandSpeed( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	float speed = 0.0;
	if (sscanf(cmd, "speed %f", &speed) == 1)
	{
		playerInstanceAamp->SetPlaybackSpeed(speed);
	}
}

void PlaybackCommand::HandleCommandBPS( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	int rate = 0;
	if (sscanf(cmd, "bps %d", &rate) == 1)
	{
		AAMPCLI_PRINTF("[AAMPCLI] Set video bitrate %d.\n", rate);
		playerInstanceAamp->SetVideoBitrate(rate);
	}
}

void PlaybackCommand::HandleCommandTuneData( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp )
{
	std::stringstream str(cmd);
	std::string token;
	std::string url;
	std::string manifestData;
	std::getline(str,token,' ');
	assert(token=="tunedata");
	if (std::getline(str,url, ' '))
	{
		mAampcli.mManifestDataUrl = url;
		if (aamp_isTuneScheme(url.c_str()))
		{
			AAMPCLI_PRINTF("[AAMPCLI] Player: url : %s \n",url.c_str());
			manifestData = getManifestData(url);
			playerInstanceAamp->Tune(url.c_str(),mAampcli.mbAutoPlay,nullptr, true, false, nullptr, true, nullptr, 0,"0259343c-cffc-4659-bcd8-97f9dd36f6b1",manifestData.c_str());
			std::string minimumUpdatePeriod = getMinimumUpdatePeriod(manifestData);
			double updateVal = ParseISO8601Duration(minimumUpdatePeriod.c_str());
			AAMPCLI_PRINTF("[AAMPCLI] Manifest minimumUpdatePeriod = %f \n",updateVal);
			int count = 4; //for testing - limit the updates to 4
			if ( updateVal != 0) //live manifest updates
			{
				while( count-- > 0)
				{
					manifestData = getManifestData(url);
					playerInstanceAamp->updateManifest(manifestData.c_str());
					std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(updateVal)));
				}
			}
		}
		else
		{
			AAMPCLI_PRINTF("[AAMP-CLI] ERROR - param '%s'\n",url.c_str());
		}
	}
	else
	{
		AAMPCLI_PRINTF("[AAMP-CLI] ERROR - expected 'tunedata <url>'\n");
	}
}void PlaybackCommand::HandleAdTesting()
{
    mAampcli.mIndexedAds = !mAampcli.mIndexedAds;
	AAMPCLI_PRINTF("[AAMPCLI] Ad Testing: %s\n", mAampcli.mIndexedAds ? "Enabled" : "Disabled");

}

/**
 * @brief Process command
 * @param cmd command
 */
bool PlaybackCommand::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	if( cmd[0]=='#' )
	{ // comment - ignore
		AAMPCLI_PRINTF( "%s\n", cmd );
	}
	else if( isCommandMatch(cmd,"parse") )
	{
		parse(&cmd[5]);
	}
	else if( isCommandMatch(cmd, "help") )
	{
		showHelp();
	}
	else if( isCommandMatch(cmd, "list") )
	{
		HandleCommandList( cmd );
	}
	else if( isCommandMatch(cmd,"autoplay") )
	{
		mAampcli.mbAutoPlay = !mAampcli.mbAutoPlay;
		AAMPCLI_PRINTF( "autoplay = %s\n", mAampcli.mbAutoPlay?"true":"false" );
	}
	else if( isCommandMatch(cmd,"contentType") )
	{
		HandleCommandContentType(cmd);
	}
	else if( isCommandMatch(cmd,"new") )
	{
		HandleCommandNew(cmd);
	}
	else if( isCommandMatch(cmd,"sleep") )
	{
		HandleCommandSleep(cmd);
	}
	else if( isCommandMatch(cmd,"select") )
	{
		HandleCommandSelect( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd,"detach") )
	{
		playerInstanceAamp->detach();
	}
	else if( isCommandMatch(cmd, "release") )
	{
		HandleCommandRelease(cmd, playerInstanceAamp );
	}
	else if( aamp_isTuneScheme(cmd) )
	{
		HandleCommandTuneLocator( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "next") )
	{
		HandleCommandNext( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "prev") )
	{
		HandleCommandPrev( cmd, playerInstanceAamp );
	}
	else if( isNumber(cmd) )
	{
		HandleCommandTuneIndex( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd,"seek") )
	{
		HandleCommandSeek(cmd,playerInstanceAamp);
	}
	else if (isCommandMatch(cmd, "slow") )
	{
		playerInstanceAamp->SetRate((float)0.5);
	}
	else if (isCommandMatch(cmd,"ff"))
	{
		HandleCommandFF( cmd, playerInstanceAamp );
	}
	else if (isCommandMatch(cmd,"rew"))
	{
		HandleCommandREW( cmd, playerInstanceAamp );
	}
	else if (strcmp(cmd, "play") == 0)
	{
		playerInstanceAamp->SetRate(1);
	}
	else if( isCommandMatch(cmd,"pause") )
	{
		HandleCommandPause(cmd,playerInstanceAamp);
	}
	else if( isCommandMatch(cmd,"speed") )
	{
		HandleCommandSpeed(cmd,playerInstanceAamp);
	}
	else if( isCommandMatch(cmd,"bps") )
	{
		HandleCommandBPS( cmd, playerInstanceAamp );
	}
	else if (isCommandMatch(cmd, "stop") )
	{
		playerInstanceAamp->Stop();
	}
	else if (isCommandMatch(cmd, "live") )
	{
		playerInstanceAamp->SeekToLive();
	}
	else if (isCommandMatch(cmd, "setconfig") )
	{
		HandleCommandSetConfig( cmd, playerInstanceAamp );
	}
	else if (isCommandMatch(cmd, "getconfig") )
	{
		HandleCommandGetConfig( cmd, playerInstanceAamp );
	}
	else if (isCommandMatch(cmd, "resetconfig") )
	{
		playerInstanceAamp->ResetConfiguration();
	}
	else if( isCommandMatch(cmd,"noisy") )
	{
		AampLogManager::lockLogLevel(false);
		AampLogManager::setLogLevel(eLOGLEVEL_INFO);
		AAMPCLI_PRINTF( "[AAMPCLI] core logging noisy\n" );
	}
	else if( isCommandMatch(cmd,"quiet") )
	{
		AampLogManager::lockLogLevel(false);
		AampLogManager::setLogLevel(eLOGLEVEL_ERROR);
		AampLogManager::lockLogLevel(true);
		AAMPCLI_PRINTF( "[AAMPCLI] core logging quiet\n" );
	}
	else if (isCommandMatch(cmd, "exit") )
	{
		HandleCommandExit();
		return false;
	}
	else if ( isCommandMatch(cmd, "customheader") )
	{
		HandleCommandCustomHeader( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "unlock") )
	{
		HandleCommandUnlock( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "lock") )
	{
		playerInstanceAamp->EnableContentRestrictions();
	}
	else if( isCommandMatch(cmd, "progress") )
	{
		mAampcli.mEnableProgressLog = mAampcli.mEnableProgressLog ? false : true;
	}
	else if( isCommandMatch(cmd, "stats") )
	{
		AAMPCLI_PRINTF("[AAMPCLI] statistics:\n%s\n", playerInstanceAamp->GetPlaybackStats().c_str());
	}
	else if( isCommandMatch(cmd,"subtec") )
	{
		HandleCommandSubtec();
	}
	else if( isCommandMatch(cmd,"history") )
	{
		// history_length is defined in the header file history.h
		for (int i = 0; i < history_length; i++)
		{
			AAMPCLI_PRINTF ("%s\n", history_get(i) ? history_get(i)->line : "NULL");
		}
	}
	else if( isCommandMatch(cmd,"auto") )
	{
		HandleCommandAuto( cmd );
	}
	else  if( isCommandMatch(cmd,"fog") )
	{
		HandleCommandFog( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "advert") )
	{
		HandleCommandAdvert( cmd, playerInstanceAamp );
	}
	else if( isCommandMatch(cmd, "scte35") )
	{
		HandleCommandScte35( cmd );
	}
	else if (isCommandMatch(cmd,"sessionid"))
	{
		HandleCommandSessionId( cmd );
	}
	else if(isCommandMatch(cmd,"tunedata"))
	{
		HandleCommandTuneData( cmd, playerInstanceAamp );
	}
	else if (isCommandMatch(cmd, "adtesting"))
	{
		HandleAdTesting();
	}
	else if (isCommandMatch(cmd, "batch"))
	{
		HandleCommandBatch();
	}
	else if (isCommandMatch(cmd, "set"))
	{
		mAampcli.doHandleAampCliCommands( cmd );
	}
	else if (isCommandMatch(cmd, "get"))
	{
		mAampcli.doHandleAampCliCommands( cmd );
	}
	else
	{
		AAMPCLI_PRINTF( "[AAMP-CLI] unmatched command: %s\n", cmd );
	}
	return true;
}

bool PlaybackCommand::isCommandMatch( const char *cmdBuf, const char *cmdName )
{
	for(;;)
	{
		char k = *cmdBuf++;
		char c = *cmdName++;
		if( !c )
		{ // cli command ends with whitespace or digit
			return (k<=' ' ) || (k>='0' && k<='9');
		}
		if( k!=c )
		{
			return false;
		}
	}
}

/**
 * @brief check if the char array is having numbers only
 * @param s
 * @retval true or false
 */
bool PlaybackCommand::isNumber(const char *s)
{
	if (*s)
	{
		if (*s == '-')
		{ // skip leading minus
			s++;
		}
		for (;;)
		{
			if (*s >= '0' && *s <= '9')
			{
				s++;
				continue;
			}
			if (*s == 0x00)
			{
				return true;
			}
			break;
		}
	}
	return false;
}

/**
 * @brief Stop mainloop execution (for standalone mode)
 */
void PlaybackCommand::termPlayerLoop()
{
	if(mAampcli.mAampGstPlayerMainLoop)
	{
		g_main_loop_quit(mAampcli.mAampGstPlayerMainLoop);
		g_thread_join(mAampcli.mAampMainLoopThread);
		PlayerCliGstTerm();
		AAMPCLI_PRINTF("[AAMPCLI] Exit\n");
	}
}

void PlaybackCommand::registerPlaybackCommands()
{
	thread_local bool runOnce = false;

	if (runOnce)
	{
		// Avoid any chance of this static member function creating another copy of the commands
		commands.clear();
		playbackCommands.clear();
	}
	else
	{
		runOnce = true;
	}

	addCommand("get help","Show 'get' commands");
	addCommand("set help","Show 'set' commands");
	addCommand("history","Show user-entered aampcli command history" );
	addCommand("batch","Execute commands line by line as batch as defined in #Home/aampcli.bat (~/aampcli.bat)" );
	addCommand("help","Show this list of available commands");
	addCommand("parse <mp4file>","mp4 box parser tool" );

	// tuning
	addCommand("autoplay","Toggle whether to autoplay (default=true)");
	addCommand("contentType <contentType>","Specify contentType to use when tuning e.g contentType LINEAR_TV");
	addCommand("list","Show virtual channel map; optionally pass a range e.g. 1-10, a start channel or -n to show the last n channels");
	addCommand("<channelNumber>","Tune specified virtual channel");
	addCommand("next","Tune next virtual channel");
	addCommand("prev","Tune previous virtual channel");
	addCommand("<url>","Tune to arbitrary locator");
	addCommand("fog <url|host=ip:port>", "'fog url' tune to arbitrary locator via fog. 'fog host=ip:port' set fog location (default: 127.0.0.1:9080)");

	addCommand("sessionid [<sid>]", "Session ID to be passed to the player with the next tune command. If called without argument, will print the current Session IDs of all players.");

	// trickplay
	addCommand("play","Continue existing playback");
	addCommand("slow","Slow Motion playback");
	addCommand("ff <x>","Fast <speed>");
	addCommand("rew <x>","Rewind <speed>");
	addCommand("pause","Pause playerback");
	addCommand("pause <s>","Schedule pause at position<s>; pass -1 to cancel");
	addCommand("seek <s> <p>","Seek to position<s>; optionally pass 1 for <p> to remain paused");
	addCommand("live","Seek to live edge");
	addCommand("stop","Stop the existing playback");

	// simulated events
	addCommand("setconfig <json>","Set the Configuration of the player using a string in json format");
	addCommand("getconfig","Get the current Configuration of the player instance");
	addCommand("resetconfig","Reset the Configuration of the player instance");
	addCommand("lock","Lock parental control");
	addCommand("unlock <t>","Unlock parental control; <t> for timed unlock in seconds>");
	addCommand("rollover","Schedule artificial pts rollover 10s after next tune");

	// background player instances
	addCommand("new","Create new player instance (in addition to default)");
	addCommand("select","Enumerate available player instances");
	addCommand("select <index>","Select player instance to use");
	addCommand("detach","Detach (lightweight stop) selected player instance");

#ifdef __APPLE__
	addCommand("subtec","Launch subtec-app and default enable cc." );
#endif

	// special
	addCommand("quiet","toggle core aamp logs (on by default");
	addCommand("sleep <ms>","Sleep <ms> milliseconds");
	addCommand("bps <x>","lock abr to bitrate <x>");
	addCommand("customheader <header>", "apply global http header on all outgoing requests" ); // TODO: move to 'set'?
	addCommand("progress","Toggle progress event logging (default=false)");
	addCommand("auto <params", "stress test with defaults: startChan(500) endChan(1000) maxTuneTime(6) playTime(15) betweenTime(15)" );
	addCommand("exit","Exit aampcli");
	addCommand("advert <params>", "manage injected advert list - 'list', 'add <url or channel in virtual channel map>', 'rm <url or index into list>'");
	addCommand("scte35 <base64>", "decode SCTE-35 signal base64 string");
	addCommand("release <playerId/playerName>", "to remove the player");
	addCommand("tunedata <url>","Tune passing a manifest buffer as a string");
	addCommand("adtesting", "toggle index based adtesting logic for testing");
}

void PlaybackCommand::addCommand(std::string command,std::string description)
{
	playbackCommands.insert(make_pair(command,description));
	commands.push_back(command);
}

#include "mp4demux.hpp"
void PlaybackCommand::parse( const char *path )
{
	while( *path == ' ' )
	{
		path++;
	}
	if( *path )
	{
		FILE *f = fopen(path,"rb");
		if( f )
		{
			fseek(f,0,SEEK_END);
			long pos = ftell(f);
			if( pos>=0 )
			{
				size_t len = (size_t)pos;
				void *ptr = malloc(len);
				if( ptr )
				{
					fseek(f,0,SEEK_SET);
					size_t rc = fread(ptr,1,len,f);
					if( rc == len )
					{
						auto mp4Demux = new Mp4Demux(true);
						// coverity[TAINTED_SCALAR]:SUPPRESS
						mp4Demux->Parse(ptr,len);
						delete mp4Demux;
					}
					free( ptr );
				}
			}
			fclose( f );
		}
		else
		{
			AAMPCLI_PRINTF( "unable to open file '%s'\n", path );
		}
	}
	else
	{
		AAMPCLI_PRINTF( "usage: parse <path>\n" );
	}
}

/**
 * @brief Show help menu with aamp command line interface
 */
void PlaybackCommand::showHelp(void)
{

	std::map<std::string,std::string>::iterator playbackCmdItr;

	AAMPCLI_PRINTF("******************************************************************************************\n");
	AAMPCLI_PRINTF("*   <command> [<arguments>]\n");
	AAMPCLI_PRINTF("*   Usage of Commands, and arguments expected\n");
	AAMPCLI_PRINTF("******************************************************************************************\n");

	for(const auto& itr:commands)
	{
		playbackCmdItr = playbackCommands.find(itr);

		if(playbackCmdItr != playbackCommands.end())
		{
			// CID:306266 - Not restoring ostream format
			std::ios_base::fmtflags orig_flags = std::cout.flags();
			std::cout << std::setw(20) << std::left << (playbackCmdItr->first).c_str() << "// "<< (playbackCmdItr->second).c_str() << "\n";
			(void)std::cout.flags(orig_flags);
		}
	}

	AAMPCLI_PRINTF("******************************************************************************************\n");
}

char * PlaybackCommand::commandRecommender(const char *text, int state)
{
	static size_t len;
	static std::vector<std::string>::iterator itr;

	if (!state)
	{
		itr = commands.begin();
		len = strlen(text);
	}

	while (itr != commands.end())
	{
		char *name = (char *) itr->c_str();
		itr++;
		if (strncmp(name, text, len) == 0)
		{
			return strdup(name);
		}
	}

	return NULL;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string PlaybackCommand::getManifestData(std::string& url)
{
	CURL* curl;
	std::string manifestData;
	if( url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://" )
	{
		auto delim = url.find('@');
		curl = curl_easy_init();
		if(curl)
		{
			if( delim != std::string::npos )
			{
				std::string range = url.substr(delim+1);
				std::string prefix = url.substr(0,delim);
				(void)curl_easy_setopt(curl, CURLOPT_URL, prefix.c_str() );
				(void)curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str() );
			}
			else
			{
				(void)curl_easy_setopt(curl, CURLOPT_URL, url.c_str() );
				(void)curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
			}
			(void)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			(void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
			(void)curl_easy_setopt(curl, CURLOPT_WRITEDATA, &manifestData);
			(void)curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
			CURLcode rc = curl_easy_perform(curl);
			if (CURLE_OK == rc)
			{
				long response_code = 0;
				(void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
				switch( response_code )
				{
					case 200:
					case 204:
					case 206:
						break;
					default:
						// http error
						AAMPCLI_PRINTF("[AAMPCLI] %s curl error code : %ld",__FUNCTION__,response_code);
						break;
				}
			}
			curl_easy_cleanup(curl);
		}
	}
	return manifestData;
}

std::string PlaybackCommand::getMinimumUpdatePeriod(const std::string& manifestData)
{
	std::string tag = "minimumUpdatePeriod=\"";
	size_t start = manifestData.find(tag);
	if (start == std::string::npos)
	{
		return "";
	}
	start += tag.length();
	size_t end = manifestData.find("\"", start);
	if (end == std::string::npos)
	{
		return "";
	}
	std::string periodStr = manifestData.substr(start, end - start);
	return periodStr;
}
