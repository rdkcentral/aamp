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
#include "Aampcli.h"
#include "AampcliPlaybackCommand.h"

extern bool gAampcliQuietLogs;
extern VirtualChannelMap mVirtualChannelMap;
extern Aampcli mAampcli;
extern void tsdemuxer_InduceRollover( bool enable );

std::map<std::string,std::string> PlaybackCommand::playbackCommands = std::map<std::string,std::string>();
std::vector<std::string> PlaybackCommand::commands(0);

/**
 * @brief Process command
 * @param cmd command
 */
bool PlaybackCommand::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	bool eventChange = false;
	char lang[MAX_LANGUAGE_TAG_LENGTH];
	int keepPaused = 0;
	int rate = 0;
	double seconds = 0;
	long unlockSeconds = 0;
	long grace = 0;
	long time = -1;
	int ms = 0;
	int playerIndex = -1;

	if( cmd[0]=='#' )
	{
		printf( "skipping comment\n" );
	}
	else if( isCommandMatch(cmd, "help") )
	{
		showHelp();
	}
	else if( isCommandMatch(cmd,"rollover") )
	{
		printf( "enabling artificial pts rollover (10s after next tune)\n" );
		tsdemuxer_InduceRollover( true );
	}
	else if( isCommandMatch(cmd, "list") )
	{
		mVirtualChannelMap.showList();
	}
	else if( isCommandMatch(cmd,"autoplay") )
	{
		mAampcli.mbAutoPlay = !mAampcli.mbAutoPlay;
		printf( "autoplay = %s\n", mAampcli.mbAutoPlay?"true":"false" );
	}
	else if( isCommandMatch(cmd,"new") )
	{
		mAampcli.newPlayerInstance();
	}
	else if( sscanf(cmd, "sleep %d", &ms ) == 1 )
	{
		if( ms>0 )
		{
			printf( "sleeping for %f seconds\n", ms/1000.0 );
			g_usleep (ms * 1000);
			printf( "sleep complete\n" );
		}
	}
	else if( sscanf(cmd, "select %d", &playerIndex ) == 1 )
	{
		if( playerIndex < mAampcli.mPlayerInstances.size() )
		{
			playerInstanceAamp = mAampcli.mPlayerInstances.at(playerIndex);
			if (playerInstanceAamp->aamp)
			{
				printf( "selected player %d (at %p)\n", playerInstanceAamp->aamp->mPlayerId, playerInstanceAamp);
				mAampcli.mSingleton=playerInstanceAamp;
			}
			else
			{
				printf( "error - player exists but is not valid/ready, playerInstanceAamp->aamp is not a valid ptr\n");
			}
		}
		else
		{
			printf( "valid range = 0..%lu\n", mAampcli.mPlayerInstances.size()-1 );
		}
	}
	else if( isCommandMatch(cmd, "select") )
	{ // List available player instances
		printf( "player instances:\n" );
		playerIndex = 0;
		for( auto playerInstance : mAampcli.mPlayerInstances )
		{
			printf( "\t%d", playerIndex++ );
			if( playerInstance == playerInstanceAamp )
			{
				printf( " (selected)");
			}
			printf( "\n ");
		}
	}
	else if( isCommandMatch(cmd,"detach") )
	{
		playerInstanceAamp->detach();
	}
	else if( playerInstanceAamp->isTuneScheme(cmd) )
	{
		playerInstanceAamp->Tune(cmd,mAampcli.mbAutoPlay);
	}
	else if( isCommandMatch(cmd, "next") )
	{
		VirtualChannelInfo *pNextChannel = mVirtualChannelMap.next();
		if (pNextChannel)
		{
			printf("[AAMPCLI] next %d: %s\n", pNextChannel->channelNumber, pNextChannel->name.c_str());
			mVirtualChannelMap.tuneToChannel( *pNextChannel, playerInstanceAamp, mAampcli.mbAutoPlay );
		}
		else
		{
			printf("[AAMPCLI] can not fetch 'next' channel, empty virtual channel map\n");
		}
	}
	else if( isCommandMatch(cmd, "prev") )
	{
		VirtualChannelInfo *pPrevChannel = mVirtualChannelMap.prev();
		if (pPrevChannel)
		{
			printf("[AAMPCLI] next %d: %s\n", pPrevChannel->channelNumber, pPrevChannel->name.c_str());
			mVirtualChannelMap.tuneToChannel( *pPrevChannel, playerInstanceAamp, mAampcli.mbAutoPlay );
		}
		else
		{
			printf("[AAMPCLI] can not fetch 'prev' channel, empty virtual channel map\n");
		}
	}
	else if( isNumber(cmd) )
	{
		int channelNumber = atoi(cmd);  // invalid input results in 0 -- will not be found

		VirtualChannelInfo *pChannelInfo = mVirtualChannelMap.find(channelNumber);
		if (pChannelInfo != NULL)
		{
			printf("[AAMPCLI] channel number: %d\n", channelNumber);
			mVirtualChannelMap.tuneToChannel( *pChannelInfo, playerInstanceAamp, mAampcli.mbAutoPlay );
		}
		else
		{
			printf("[AAMPCLI] channel number: %d was not found\n", channelNumber);
		}
	}
	else if (sscanf(cmd, "seek %lf %d", &seconds, &keepPaused) >= 1)
	{
		bool seekWhilePaused = (keepPaused==1);
		playerInstanceAamp->Seek(seconds, seekWhilePaused );
	}
	else if (isCommandMatch(cmd, "slow") )
	{
		playerInstanceAamp->SetRate((float)0.5);
	}
	else if (sscanf(cmd, "ff %d", &rate) == 1)
	{
		playerInstanceAamp->SetRate((float)rate);
	}
	else if (strcmp(cmd, "play") == 0)
	{
		playerInstanceAamp->SetRate(1);
	}
	else if (sscanf(cmd, "pause %lf", &seconds) == 1)
	{
		playerInstanceAamp->PauseAt(seconds);
	}
	else if (strcmp(cmd, "pause") == 0)
	{
		playerInstanceAamp->SetRate(0);
	}
	else if (sscanf(cmd, "rew %d", &rate) == 1)
	{
		playerInstanceAamp->SetRate((float)(-rate));
	}
	else if (sscanf(cmd, "bps %d", &rate) == 1)
	{
		printf("[AAMPCLI] Set video bitrate %d.\n", rate);
		playerInstanceAamp->SetVideoBitrate(rate);
	}
	else if (isCommandMatch(cmd, "flush") )
	{
		playerInstanceAamp->aamp->mStreamSink->Flush();
	}
	else if (isCommandMatch(cmd, "stop") )
	{
		playerInstanceAamp->Stop();
		tsdemuxer_InduceRollover(false);
	}
	else if (isCommandMatch(cmd, "underflow") )
	{
		playerInstanceAamp->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, eMEDIATYPE_VIDEO);
	}
	else if (isCommandMatch(cmd, "retune") )
	{
		playerInstanceAamp->aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, eMEDIATYPE_VIDEO);
	}
	else if (isCommandMatch(cmd, "live") )
	{
		playerInstanceAamp->SeekToLive();
	}
	else if( isCommandMatch(cmd,"quiet") )
	{
		gAampcliQuietLogs = !gAampcliQuietLogs;
		printf("[AAMPCLI] core logging: %s\n", gAampcliQuietLogs?"QUIET":"NORMAL" );
	}
	else if (isCommandMatch(cmd, "exit") )
	{
		playerInstanceAamp = NULL;
		for( auto playerInstance : mAampcli.mPlayerInstances )
		{
			playerInstance->Stop();
			SAFE_DELETE( playerInstance );
		}
		termPlayerLoop();
		return false;	//to exit
	}
	else if( isCommandMatch(cmd, "customheader") )
	{
		std::vector<std::string> headerValue;
		printf("[AAMPCLI] customheader Command is %s\n" , cmd);
		playerInstanceAamp->AddCustomHTTPHeader(
												"", // headerName(?)
												headerValue,
												false); // isLicenseHeader
	}
	else if( sscanf(cmd, "unlock %ld", &unlockSeconds) >= 1 )
	{
		printf("[AAMPCLI] unlocking for %ld seconds\n" , unlockSeconds);
		if(-1 == unlockSeconds)
			grace = -1;
		else
			time = unlockSeconds;
		playerInstanceAamp->DisableContentRestrictions(grace, time, eventChange);
	}
	else if( isCommandMatch(cmd, "unlock") )
	{
		printf("[AAMPCLI] unlocking till next program change\n");
		eventChange = true;
		playerInstanceAamp->DisableContentRestrictions(grace, time, eventChange);
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
		printf("[AAMPCLI] statistics:\n%s\n", playerInstanceAamp->GetPlaybackStats().c_str());
	}
	else if( isCommandMatch(cmd,"subtec") )
	{
#ifdef __APPLE__
		mAampcli.mSingleton->SetCCStatus(true);
		system( "cd ../..;bash install-aamp.sh subtec&\n");
#endif
	}
	else if( isCommandMatch(cmd,"history") )
	{
		// history_length is defined in the header file history.h
		for (int i = 0; i < history_length; i++)
		{
			printf ("%s\n", history_get(i+1)->line);
		}
	}
	else if( isCommandMatch(cmd,"auto") )
	{
		int start=500, end=1000;
		int maxTuneTimeS = 6;
		int playTimeS = 15;
		int betweenTimeS = 15;
		int matched = sscanf(cmd, "auto %d %d %d %d %d", &start, &end, &maxTuneTimeS, &playTimeS, &betweenTimeS );
		mAampcli.doAutomation( start, end, maxTuneTimeS, playTimeS, betweenTimeS );
	}
	else
	{
		printf( "[AAMP-CLI] unmatched command: %s\n", cmd );
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
		{
			return (k<=' ' ); // buf ends with whitespace
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
		gst_deinit ();
		printf("[AAMPCLI] %s(): Exit\n", __FUNCTION__);
	}
}

void PlaybackCommand::registerPlaybackCommands()
{
	addCommand("get help","Show 'get' commands");
	addCommand("set help","Show 'set' commands");
	addCommand("history","Show user-entered aampcli command history" );
	addCommand("help","Show this list of available commands");

	// tuning
	addCommand("autoplay","Toggle whether to autoplay (default=true)");
	addCommand("list","Show virtual channel map");
	addCommand("<channelNumber>","Tune specified virtual channel");
	addCommand("next","Tune next virtual channel");
	addCommand("prev","Tune previous virtual channel");
	addCommand("<url>","Tune to arbitrary locator");

	// trickplay
	addCommand("play","Continue existing playback");
	addCommand("slow","Slow Motion playback");
	addCommand("ff <x>","Fast <speed>; up to 128x");
	addCommand("rew <y>","Rewind <speed>; up to 128x");
	addCommand("pause","Pause playerback");
	addCommand("pause <s>","Schedule pause at position<s>; pass -1 to cancel");
	addCommand("seek <s> <p>","Seek to position<s>; optionally pass 1 for <p> to remain paused");
	addCommand("live","Seek to live edge");
	addCommand("stop","Stop the existing playback");

	// simulated events
	addCommand("retune","Retune to current locator");
	addCommand("flush","Flush AV pipeline");
	addCommand("underflow","Simulate underflow");
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
}

void PlaybackCommand::addCommand(std::string command,std::string description)
{
	playbackCommands.insert(make_pair(command,description));
	commands.push_back(command);
}

/**
 * @brief Show help menu with aamp command line interface
 */
void PlaybackCommand::showHelp(void)
{

	std::map<std::string,std::string>::iterator playbackCmdItr;

	printf("******************************************************************************************\n");
	printf("*   <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");

	for(auto itr:commands)
	{
		playbackCmdItr = playbackCommands.find(itr);

		if(playbackCmdItr != playbackCommands.end())
		{
			std::cout << std::setw(20) << std::left << (playbackCmdItr->first).c_str() << "// "<< (playbackCmdItr->second).c_str() << "\n";
		}
	}

	printf("******************************************************************************************\n");
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
