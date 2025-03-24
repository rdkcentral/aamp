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
 * @file AampcliPlaybackCommand.h
 * @brief AAMPcliPlaybackCommand header file
 */

#ifndef AAMPCLIPLAYBACKCOMMAND_H
#define AAMPCLIPLAYBACKCOMMAND_H

#include "AampcliCommand.h"

typedef struct AdvertInfo
{
	std::string url;
	std::string adBreakId;
	AdvertInfo():url(),adBreakId(){};
} AdvertInfo;

class PlaybackCommand : public Command
{
	
public:
	static void registerPlaybackCommands();
	static char *commandRecommender(const char *text, int state);
	static bool isCommandMatch( const char *cmdBuf, const char *cmdName );
	static bool isNumber(const char *s);
	static void showHelp(void);
	void termPlayerLoop();
	bool execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp) override;
	PlayerInstanceAAMP * findPlayerInstance( const char *playerRef );
	static std::string getManifestData(std::string& url);
	std::string getMinimumUpdatePeriod(const std::string& manifestData);
	
private:
	void getRange(const char* cmd, unsigned long& start, unsigned long& end, unsigned long& tail);
	static void addCommand(std::string command,std::string description);
	static std::vector<std::string> commands;
	static std::map<std::string,std::string> playbackCommands;
	
	void HandleCommandList( const char *cmd );
	void HandleCommandContentType( const char *cmd );
	void HandleCommandNew( const char *cmd );
	void HandleCommandSelect( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandRelease( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandSleep( const char *cmd );
	void HandleCommandTuneLocator( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandNext( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandPrev( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandTuneIndex( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandSetConfig( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandGetConfig( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandExit( void );
	void HandleCommandCustomHeader( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandSubtec( void );
	void HandleCommandUnlock( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandAuto( const char *cmd );
	void HandleCommandFog( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandAdvert( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandScte35( const char *cmd );
	void HandleCommandSessionId( const char *cmd );
	void HandleCommandSeek( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandFF( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandREW( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandPause(const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandSpeed( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandBPS( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp );
	void HandleCommandTuneData( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp);
	void HandleAdTesting();
	void HandleCommandBatch();
};

#endif // AAMPCLIPLAYBACKCOMMAND_H
