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
 * @file TuneSmokeTest.h
 * @brief TuneSmokeTest header file
 */

#ifndef SCRIPTEDSMOKETEST_H
#define SCRIPTEDSMOKETEST_H

#include "gtest/gtest.h"
#include "priv_aamp.h"

class testInformation;

class ScriptedSmokeTest
{
    private:
        static std::map<std::string, std::string> taggedURLList;
		static std::map<std::string, std::string> tagMap;
		static uint32_t currentPlayerId;

		static void getTestUrls(std::string &filename);
		static bool applyTestIterations(std::string &filename, std::vector<testInformation> &testList);
        static bool getIterationInfo(std::ifstream &script, int iteration);

		static bool process_tune(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_stop(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_detach(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_setrate(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_ff(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_rew(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_pause(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_play(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_seek(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_async(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_config(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_setaudiotrack(std::stringstream &args, PlayerInstanceAAMP *player);
		static bool process_settextrack(std::stringstream &args, PlayerInstanceAAMP *player);

		static PlayerInstanceAAMP *getCurrentPlayer();
		static ScriptedSmokeTestEventListener *getCurrentListener();

	public:
		static bool getIsNumber(std::string &value, bool allowSigned = true, bool allowDouble = true);

		static bool getValueParameter(std::stringstream &stringStream, std::string &name, std::string &value, 
		                              bool assertValue = true, char delim = ' ');
		static bool getValueParameter(std::string &item, std::string &name, std::string &value, bool assertValue = true);
        static bool getValueParameter(std::stringstream &line, const char *name, std::string &value);
		static bool getIntParameter(std::stringstream &stringStream, int &value);
		static bool getUintParameter(std::stringstream &stringStream, uint32_t &value);
		static bool getEnvVarValue(std::string &param, std::string &name, std::string &value);
		static bool getEnvVarValue(std::string &param, std::string &name);
		static bool getEnvVar(std::string &name, std::string &value);
		static bool getInteger(std::string &text, int &value);
		static bool getInteger(std::string &text, uint32_t &value);
		static bool getInteger(std::string &text, double &value);

		static std::vector<testInformation> getTestInformation(const char *testScript = NULL);
		static bool runScript(const char *script, int iteration = 0, bool gtest = true);
    	static bool testScript(const char *testScript);
};

#endif // SCRIPTEDSMOKETEST_H
