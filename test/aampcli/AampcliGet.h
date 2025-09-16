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
 * @file AampcliGet.h
 * @brief AampcliGet header file
 */

#ifndef AAMPCLIGET_H
#define AAMPCLIGET_H

#include <cstring>
#include "main_aamp.h"
#include "AampcliCommand.h"

typedef struct GetCommandInfo{
	GetCommandInfo() : value(0), description("") {}
	int value;
	std::string description;
}getCommandInfo;

class Get : public Command {

	public:
		static void registerGetCommands();
		static char *getCommandRecommender(const char *text, int state);
		void ShowHelpGet();
		bool execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp) override;
	private:
		static void addCommand(int value,std::string command,std::string description);
		static std::vector<std::string> commands;
		static std::map<std::string,getCommandInfo> getCommands;
		static std::map<std::string,std::string> getNumCommands;
};

#endif // AAMPCLIGET_H
