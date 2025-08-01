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
 * @file AampcliGet.cpp
 * @brief Aampcli Get command handler
 */
#include <iomanip>
#include "AampcliGet.h"

std::map<std::string,getCommandInfo> Get::getCommands = std::map<std::string,getCommandInfo>();
std::vector<std::string> Get::commands(0);

bool Get::execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	int opt, value1, value2;
	char command[100];
	int getCmd = 0;
	if( sscanf(cmd, "get %s", command) == 1 )
	{
		const bool is_digit {static_cast<bool>(isdigit(command[0]))};

		if(is_digit)
		{
			getCmd = atoi(command);
		}
		else
		{
			auto getCmdItr = getCommands.find(command);

			if(getCmdItr != getCommands.end())
			{
				getCmd = getCmdItr->second.value;
			}
		}
		if( strcmp(command,"help")==0)
		{
			ShowHelpGet();
		}
		else
		{
			switch(getCmd){
				case 32:
					printf("[AAMPCLI] GETTING AVAILABLE THUMBNAIL TRACKS: %s\n", playerInstanceAamp->GetAvailableThumbnailTracks().c_str() );
					break;

				case 1:
					printf("[AAMPCLI] GETTING CURRENT STATE: %d\n", (int) playerInstanceAamp->GetState());
					break;

				case 33:
				{
					int strip_formatting = 0;

					if (is_digit)
					{
						sscanf(cmd, "get %d %d %d %d", &opt, &value1, &value2, &strip_formatting);
					}
					else
					{
						sscanf(cmd, "get thumbnailData %d %d %d", &value1, &value2, &strip_formatting);
					}
					std::string json = playerInstanceAamp->GetThumbnails(value1, value2);
					if (strip_formatting)
					{
						//strip formatting so easy to read from l2test
						json.erase(remove(json.begin(), json.end(), '\n'), json.end());
						json.erase(remove(json.begin(), json.end(), '\t'), json.end());
						printf("[AAMPCLI] GETTING THUMBNAIL TIME RANGE DATA %s\n", json.c_str());
					}
					else
					{
						printf("[AAMPCLI] GETTING THUMBNAIL TIME RANGE DATA for duration (%d,%d): %s\n complete.\n",
							   value1, value2, json.c_str());
					}
				}
					break;

				case 24:
					printf("[AAMPCLI] CURRENT AUDIO TRACK NUMBER: %d\n", playerInstanceAamp->GetAudioTrack() );
					break;

				case 25:
					printf("[AAMPCLI] INITIAL BUFFER DURATION: %d\n", playerInstanceAamp->GetInitialBufferDuration() );
					break;

				case 26:
					printf("[AAMPCLI] CURRENT AUDIO TRACK INFO: %s\n", playerInstanceAamp->GetAudioTrackInfo().c_str() );
					break;

				case 27:
					printf("[AAMPCLI] CURRENT TEXT TRACK INFO: %s\n", playerInstanceAamp->GetTextTrackInfo().c_str() );
					break;

				case 28:
					printf("[AAMPCLI] CURRENT PREPRRED AUDIO PROPERTIES: %s\n", playerInstanceAamp->GetPreferredAudioProperties().c_str() );
					break;

				case 29:
					printf("[AAMPCLI] CURRENT PREPRRED TEXT PROPERTIES: %s\n", playerInstanceAamp->GetPreferredTextProperties().c_str() );
					break;

				case 30:
					printf("[AAMPCLI] CC VISIBILITY STATUS: %s\n",playerInstanceAamp->GetCCStatus()?"ENABLED":"DISABLED");
					break;

				case 31:
					printf("[AAMPCLI] CURRENT TEXT TRACK: %d\n", playerInstanceAamp->GetTextTrack() );
					break;

				case 20:
					printf("[AAMPCLI] AVAILABLE AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(false).c_str() );
					break;

				case 34:
					printf("[AAMPCLI] AVAILABLE VIDEO TRACKS: %s\n", playerInstanceAamp->GetAvailableVideoTracks().c_str() );
					break;

				case 35:
					printf( "[AAMPCLI] LIVE: %s\n", playerInstanceAamp->IsLive()? "TRUE": "FALSE" );
					break;

				case 36:
						printf("[AAMPCLI] VIDEO PLAYBACK QUALITY: %s\n", playerInstanceAamp->GetVideoPlaybackQuality().c_str() );
					break;
					   
				case 21:
					printf("[AAMPCLI] ALL AUDIO TRACKS: %s\n", playerInstanceAamp->GetAvailableAudioTracks(true).c_str() );
					break;

				case 23:
					printf("[AAMPCLI] ALL TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(true).c_str() );
					break;

				case 22:
					printf("[AAMPCLI] AVAILABLE TEXT TRACKS: %s\n", playerInstanceAamp->GetAvailableTextTracks(false).c_str() );
					break;

				case 2:
					printf("[AAMPCLI] CURRRENT AUDIO LANGUAGE = %s\n",
							playerInstanceAamp->GetAudioLanguage().c_str());
					break;

				case 3:
					printf("[AAMPCLI] CURRRENT DRM  = %s\n",
							playerInstanceAamp->GetDRM().c_str());
					break;

				case 4:
					printf("[AAMPCLI] PLAYBACK POSITION = %lf\n",
							playerInstanceAamp->GetPlaybackPosition());
					break;

				case 5:
					printf("[AAMPCLI] PLAYBACK DURATION = %lf\n",
							playerInstanceAamp->GetPlaybackDuration());
					break;

				case 6:
					printf("[AAMPCLI] CURRENT VIDEO PROFILE BITRATE = %ld\n",
							playerInstanceAamp->GetVideoBitrate());
					break;

				case 7:
					printf("[AAMPCLI] INITIAL BITRATE = %ld \n",
							playerInstanceAamp->GetInitialBitrate());
					break;

				case 8:
					printf("[AAMPCLI] INITIAL BITRATE 4K = %ld \n",
							playerInstanceAamp->GetInitialBitrate4k());
					break;

				case 9:
					printf("[AAMPCLI] MINIMUM BITRATE = %ld \n",
							playerInstanceAamp->GetMinimumBitrate());
					break;

				case 10:
					printf("[AAMPCLI] MAXIMUM BITRATE = %ld \n",
							playerInstanceAamp->GetMaximumBitrate());
					break;

				case 11:
					printf("[AAMPCLI] AUDIO BITRATE = %ld\n",
							playerInstanceAamp->GetAudioBitrate());
					break;

				case 12:
					printf("[AAMPCLI] Video Zoom mode: %s\n",
							(playerInstanceAamp->GetVideoZoom())?"None(Normal)":"Full(Enabled)");
					break;

				case 13:
					printf("[AAMPCLI] Video Mute status:%s\n",
							(playerInstanceAamp->GetVideoMute())?"ON":"OFF");
					break;

				case 14:
					printf("[AAMPCLI] AUDIO VOLUME = %d\n",
							playerInstanceAamp->GetAudioVolume());
					break;

				case 15:
					printf("[AAMPCLI] PLAYBACK RATE = %d\n",
							playerInstanceAamp->GetPlaybackRate());
					break;

				case 16:
					{
						std::vector<long int> videoBitrates;
						std::string temp = "[AAMPCLI] VIDEO BITRATES = [ ";
						videoBitrates = playerInstanceAamp->GetVideoBitrates();
						for(int i=0; i < videoBitrates.size(); i++){
							if( i ) temp += ", ";
							temp += std::to_string(videoBitrates[i]);
						}
						temp += " ]";
						printf( "%s\n", temp.c_str() );
						break;
					}

				case 17:
					{
						std::vector<long int> audioBitrates;
						std::string temp = "[AAMPCLI] AUDIO BITRATES = [ ";
						audioBitrates = playerInstanceAamp->GetAudioBitrates();
						for(int i=0; i < audioBitrates.size(); i++){
							if( i ) temp += ", ";
							temp += std::to_string(audioBitrates[i]);
						}
						temp += " ]";
						printf( "%s\n", temp.c_str() );
						break;
					}
				case 18:
					{
						std::string preferredLanguages = playerInstanceAamp->GetPreferredLanguages();
						printf("[AAMPCLI] PREFERRED LANGUAGES = \"%s\"\n", preferredLanguages.c_str() );
						break;
					}

				case 19:
					{
						printf("[AAMPCLI] RAMP DOWN LIMIT= %d\n", playerInstanceAamp->GetRampDownLimit());
						break;
					}

				case 37:
					{
						printf("[AAMPCLI] WEBVTT/TTML support: \"%s\"\n", playerInstanceAamp->IsOOBCCRenderingSupported() ? "Enabled" : "Disabled");
						break;
					}

				default:
					printf("[AAMPCLI] Invalid get command %s\n", cmd);
					break;
			}

		}
	}
	else
	{
		printf("[AAMPCLI] Invalid get command = %s\n", cmd);
	}

	return true;
}

/**
 * @brief Show help menu with aamp command line interface
 */
void Get::registerGetCommands()
{
	thread_local bool runOnce = false;

	if (runOnce)
	{
		// Avoid any chance of this static member function creating another copy of the commands
		commands.clear();
		getCommands.clear();
	}
	else
	{
		runOnce = true;
	}

	addCommand(1,"currentState","Get current player state");
	addCommand(2,"currentAudioLan","Get Current audio language");
	addCommand(3,"currentDrm","Get Current DRM");
	addCommand(4,"playbackPosition","Get Current Playback position");
	addCommand(5,"playbackDuration","Get Playback Duration");
	addCommand(6,"videoBitrate","Get current video bitrate");
	addCommand(7,"initialBitrate","Get Initial Bitrate");
	addCommand(8,"initialBitrate4k","Get Initial Bitrate 4K");
	addCommand(9,"minimumBitrate","Get Minimum Bitrate");
	addCommand(10,"maximumBitrate","Get Maximum Bitrate");
	addCommand(11,"audioBitrate","Get current Audio bitrate");
	addCommand(12,"videoZoom","Get Video Zoom mode");
	addCommand(13,"videoMute","Get Video Mute status");
	addCommand(14,"audioVolume","Get current Audio volume");
	addCommand(15,"playbackRate","Get Current Playback rate");
	addCommand(16,"videoBitrates","Get Video bitrates supported");
	addCommand(17,"audioBitrates","Get Audio bitrates supported");
	addCommand(18,"currentPreferredLanguages","Get Current preferred languages");
	addCommand(19,"rampDownLimit","Get number of  Ramp down limit during playback");
	addCommand(20,"availableAudioTracks","Get Available Audio Tracks");
	addCommand(21,"allAvailableAudioTracks","Get All Available Audio Tracks information from manifest");
	addCommand(22,"availableTextTracks","Get Available Text Tracks");
	addCommand(23,"allAvailableTextTracks","Get All Available Text Tracks information from manifest");
	addCommand(24,"audioTrack","Get Audio Track");
	addCommand(25,"initialBufferDuration","Get Initial Buffer Duration( in sec)");
	addCommand(26,"audioTrackInfo","Get current Audio Track information in json format");
	addCommand(27,"textTrackInfo","Get current Text Track information in json format");
	addCommand(28,"preferredAudioProperties","Get current Preferred Audio properties in json format");
	addCommand(29,"preferredTextProperties","Get current Preferred Text properties in json format");
	addCommand(30,"ccStatus","Get CC Status");
	addCommand(31,"textTrack","Get Text Track");
	addCommand(32,"thumbnailConfig","Get Available ThumbnailTracks");
	addCommand(33,"thumbnailData","Get Thumbnail timerange <int_startpos int_endpos [int_format]> ");
	addCommand(34,"availableVideoTracks","Get All Available Video Tracks information from manifest");
	addCommand(35,"live","Report if playback is logically from live edge");
	addCommand(36,"playbackQuality","Get playback quality info");
	addCommand(37,"isOOBCCRenderingSupported","Get the status of out of band caption rendering support");
	commands.push_back("help");
}

void Get::addCommand(int value,std::string command,std::string description)
{
	getCommandInfo lCmdInfo;
	lCmdInfo.value = value;
	lCmdInfo.description = description;

	getCommands.insert(std::make_pair(command,lCmdInfo));
	commands.push_back(command);
}

/**
 * @brief Display Help menu for get
 * @param none
 */
void Get::ShowHelpGet(){

	printf("******************************************************************************************\n");
	printf("*   get <command> [<arguments>]\n");
	printf("*   Usage of Commands, and arguments expected\n");
	printf("******************************************************************************************\n");

	if(!commands.empty())
	{
		for(const auto& itr:commands)
		{
			auto getCmdItr = getCommands.find(itr);
			if(getCmdItr != getCommands.end())
			{
				std::cout << "get " << std::right << std::setw(2) << (getCmdItr->second).value << " / " << std::setw(35) << std::left << (getCmdItr->first).c_str() << "// "<< (getCmdItr->second.description).c_str() << "\n";
			}
		}
	}

	printf("****************************************************************************\n");
}

char * Get::getCommandRecommender(const char *text, int state)
{
	char *name;
	static size_t len;
	static std::vector<std::string>::iterator itr;

	if (!state)
	{
		itr = commands.begin();
		len = strlen(text);
	}

	while (itr != commands.end())
	{
		name = (char *) itr->c_str();
		itr++;
		if (strncmp(name, text, len) == 0)
		{
			return strdup(name);
		}
	}

	return NULL;
}
