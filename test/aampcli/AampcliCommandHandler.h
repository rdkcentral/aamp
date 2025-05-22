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
 * @file AampcliCommandHandler.h
 * @brief AampcliCommandHandler header file
 */

#ifndef AAMPCLICOMMANDHANDLER_H
#define AAMPCLICOMMANDHANDLER_H

#include <string>
#include <map>
#include <vector>

#include "priv_aamp.h"
#include "AampcliCommand.h"
#include "AampcliGet.h"
#include "AampcliSet.h"
#include "AampcliPlaybackCommand.h"

class CommandHandler
{
	private:
		Set mSet{};
		Get mGet{};
		PlaybackCommand mPlaybackCommand{};

		std::map<std::string, Command*> mCommandMap{};

	public:
		CommandHandler();

		void registerAampcliCommands();
		void registerCommand(const std::string& commandName, Command* command);
		bool dispatchAampcliCommands( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp);
		void registerAllCommands();
		static char **commandCompletion(const char *text, int start, int end);
};

#endif // AAMPCLICOMMANDHANDLER_H
