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
 * @file AampcliCommandHandler.cpp
 * @brief Aampcli Command register and handler
 */

#include <stdio.h>  // required by readline
#include <readline/readline.h>
#include "AampcliCommandHandler.h"

CommandHandler::CommandHandler()
{
	mCommandMap = {
		{"set", &mSet},
		{"get", &mGet},
		{"default", &mPlaybackCommand}
	};
}

void CommandHandler::registerAampcliCommands()
{
	registerAllCommands();
}

bool CommandHandler::dispatchAampcliCommands( const char *cmdBuf, PlayerInstanceAAMP *playerInstanceAamp )
{
	for( auto commandInfo : mCommandMap )
	{
		if( PlaybackCommand::isCommandMatch(cmdBuf, commandInfo.first.c_str() ) )
		{
			Command* l_Command = commandInfo.second;
			return l_Command->execute(cmdBuf,playerInstanceAamp);
		}
	}
	auto commandInfo = mCommandMap.find("default");
	Command* l_Command = commandInfo->second;
	return l_Command->execute(cmdBuf,playerInstanceAamp);
}

void CommandHandler::registerAllCommands()
{
	PlaybackCommand::registerPlaybackCommands();
	Get::registerGetCommands();
	Set::registerSetCommands();
}

char ** CommandHandler::commandCompletion(const char *text, int start, int end)
{
	char *buffer = rl_line_buffer;

	rl_attempted_completion_over = 1;

	if(strncmp(buffer, "get", 3) == 0)
	{
		return rl_completion_matches(text, Get::getCommandRecommender);
	}
	else if (strncmp(buffer, "set", 3) == 0)
	{
		return rl_completion_matches(text, Set::setCommandRecommender);
	}
	else
	{
		return rl_completion_matches(text, PlaybackCommand::commandRecommender);
	}

}
