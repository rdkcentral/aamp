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
 * @file AampcliSmokeTest.h
 * @brief AampcliSmokeTest header file
 */

#ifndef AAMPCLISMOKETEST_H
#define AAMPCLISMOKETEST_H

#include "priv_aamp.h"
#include "AampcliCommand.h"

class SmokeTest : public Command {

	public:

		static std::map<std::string, std::string> smokeTestUrls;
		void vodTune(const char *stream);
		void liveTune(const char *stream);
		void loadSmokeTestUrls();
		bool createTestFilePath(std::string &filePath);
		bool execute(const char *cmd, PlayerInstanceAAMP *playerInstanceAamp) override;
};

#endif // AAMPCLISMOKETEST_H
