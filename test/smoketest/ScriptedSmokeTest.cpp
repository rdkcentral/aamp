/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
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
 * @file ScriptedSmokeTest.cpp
 * @brief Scripted SmokeTest cases 
 */

#include <time.h>
#include "AampSmokeTestPlayer.h"
#include "ScriptedSmokeTestEventListener.h"
#include "ScriptedSmokeTest.h"

#include <fstream>
#include <dirent.h>

extern AampPlayer mAampPlayer;

#define SCRIPT_TEST_URL_FILE "scriptTestUrls.txt"

std::map<std::string, std::string> ScriptedSmokeTest::taggedURLList;
std::map<std::string, std::string> ScriptedSmokeTest::tagMap;
uint32_t ScriptedSmokeTest::currentPlayerId = 0;


//////////////////////////////////////////////////////////////////////
// This class contains the information for each test that is run
//
class testInformation
{
public:
	testInformation(const std::string &filename = ""): 
		file(filename), 
		name(),
		iteration(0)
	{
	}

	std::string file; 		// the filename of the script
	std::string name; 		// the name of the test
	int iteration;
};

const bool operator==(const testInformation& lhs, const std::string &rhs)
{
    return lhs.name == rhs;
}

std::ostream& operator<<(std::ostream& os, const testInformation& info)
{
	if (info.name != "")
	{
		os << info.file << ", test: " << info.name << ", " << info.iteration;
	}
	else if (info.iteration)
	{
		os << info.file << ", iteration: " << info.iteration;
	}
	else
	{
		os << info.file;
	}
	return os;
}



PlayerInstanceAAMP *ScriptedSmokeTest::getCurrentPlayer()
{
	return mAampPlayer.getPlayer(currentPlayerId);
}

ScriptedSmokeTestEventListener *ScriptedSmokeTest::getCurrentListener()
{
	return dynamic_cast<ScriptedSmokeTestEventListener*>(mAampPlayer.getListener(currentPlayerId));
}


/**
 * @brief Run just the specified test
 * @param[in] testScript - the filename of the test to run
 * @retval true
 */
bool ScriptedSmokeTest::testScript(const char *testScript)
{
	std::vector<testInformation> testInfo = getTestInformation(testScript);
	if (testInfo.size() == 0)
	{
		printf("ERROR: unrecognised script name '%s'", testScript);
		return false;
	}

	for (auto info : testInfo)
	{
		printf("\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n>>> Running script %s (%s, %d)\n", 
			   info.file.c_str(), info.name.c_str(), info.iteration);
		runScript(info.file.c_str(), info.iteration, false);
	}
	return true;
}


/////////////////////////////////////////////////////////////////////////////////////
// Some utility fnctions for reading the script
//

/**
 * @brief checks a string to see if it is number
 * @param[in] value - string value
 * @param[in] allowSigned - true if the number can be signed
 * @param[in] allowDouble - true if the numer is a double (can have a decimal point)
 * @retval true if the string is a number
 */
bool ScriptedSmokeTest::getIsNumber(std::string &value, bool allowSigned, bool allowDouble)
{
	if (!value.empty())
	{
		std::string::const_iterator iter = value.begin();
		int decimalPoints = 0;

		if (allowSigned)
		{
			if (*iter == '-')
			{
				iter++; // ignore negative char
			}
		}

		// Check each char is a digit
		for ( ; iter != value.end(); iter++)
		{
			if (allowDouble && (*iter == '.') && (decimalPoints == 0))
			{
				// If we are looking for a double, allow a single decimal point
				decimalPoints++;
			}
			else if (!std::isdigit(*iter))
			{
				break;
			}
		}
		if (iter == value.end())
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief get an integer value from a string
 * @param[in] text - string value
 * @param[out] value - the integer value derived from the string
 * @param[in] allowSigned - true if the integer can be signed
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getInteger(std::string &text, int &value)
{
	if (getIsNumber(text, true, false))
	{
		value = std::stoi(text);
		return true;
	}
	else
	{
		printf("%s:%d: ERROR: '%s' is not a valid int!\n",__FUNCTION__,__LINE__, text.c_str());
	}
	return false;
}

/**
 * @brief get an unsigned integer value from a string
 * @param[in] text - string value
 * @param[out] value - the unsigned integer value derived from the string
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getInteger(std::string &text, uint32_t &value)
{
	if (getIsNumber(text, false, false))
	{
		value = std::stoi(text);
		return true;
	}
	else
	{
		printf("%s:%d: ERROR: '%s' is not a valid uint!\n",__FUNCTION__,__LINE__, text.c_str());
	}
	return false;
}	

/**
 * @brief get a double value from a string
 * @param[in] text - string value
 * @param[out] value - the double value derived from the string
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getInteger(std::string &text, double &value)
{
	if (getIsNumber(text))
	{
		value = std::stod(text);
		return true;
	}
	else
	{
		printf("%s:%d: ERROR: '%s' is not a valid double!\n",__FUNCTION__,__LINE__, text.c_str());
	}
	return false;
}	

/**
 * @brief potential envornment variable - '$name' or '$name(value)'
 * @param[in] stringStream - string parameter
 * @param[out] name - the name extracted (or env var value if exported)
 * @param[out] value - the string value extracted
 * @retval true if the param was an environment variable (atsrts with '$')
 */
bool ScriptedSmokeTest::getEnvVarValue(std::string &param, std::string &name, std::string &value)
{
	if (param[0] == '$')
	{
		std::string temp = param;
		temp.erase(0, 1);
		return getValueParameter(temp, name, value, false);
	}
	return false;
}

/**
 * @brief potential envornment variable - '$name'
 * @param[in] stringStream - string parameter
 * @param[out] name - the name extracted (or env var value if exported)
 * @retval true if the param was an environment variable (atsrts with '$')
 */
bool ScriptedSmokeTest::getEnvVarValue(std::string &param, std::string &name)
{
	std::string value;
	return getEnvVarValue(param, name, value);
}

/**
 * @brief get environment variable
 * @param[in] name - the name of the variable
 * @param[out] value - the value of the variable
 * @retval true if 'name' was an exported environment variable
 */
bool ScriptedSmokeTest::getEnvVar(std::string &name, std::string &value)
{
	char *envvar = getenv(name.c_str());
	if (envvar)
	{
		printf("%s:%d: Got variable '%s' from environment: '%s'\n",__FUNCTION__,__LINE__, name.c_str(), envvar);
		value = envvar;
		return true;
	}
	return false;
}

/**
 * @brief get a named string value of the form 'name(value)'
 * @param[in] stringStream - stream to extract the value from
 * @param[out] name - the name value extracted
 * @param[in, out] value - the string value extracted
 * @param[in] delim - the stream delimiting character
 * @param[in] assertValue - true if there must be a value - 'n(v)', false if it is optional - 'n'
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::stringstream &stringStream, std::string &name, std::string &value, 
                                          bool assertValue, char delim)
{
	std::string item;
	bool retval = false;
	if (std::getline(stringStream, item, delim)) // get the item
	{
		// We are looking for synyax 'name(value)'
		std::stringstream itemStream(item);
		if (std::getline(itemStream, name, '('))
		{
			if (itemStream.eof())
			{
				value.clear();
				retval = !assertValue; // ok if the value is optional
			}
			else if (std::getline(itemStream, value, '\0'))
			{
				value.erase(value.rfind(')')); // remove end ')'
				retval = !value.empty();
			}
		}
	}
	return retval;
}

/**
 * @brief get a named string value of the form 'name(value)'
 * @param[in] item - stream to extract the value from
 * @param[in, out] name - the name value extracted
 * @param[in, out] value - the string value extracted
 * @param[in] assertValue - true if there must be a value - 'n(v)', false if it is optional - 'n'
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::string &item, std::string &name, std::string &value, bool assertValue)
{
	std::stringstream stream(item);
	return getValueParameter(stream, name, value, assertValue);
}

/**
 * @brief get a named string value of the form 'name(value)' with a specified name
 * @param[in] stringStream - stream to extract the value from
 * @param[in] name - the name to check for
 * @param[out] value - the string value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::stringstream &stringStream, const char *name, std::string &value)
{
	std::string itemName;
	if (getValueParameter(stringStream, itemName, value))
	{
		if (itemName == name)
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief get an integer value from a string stream
 * @param[in] stringStream - stream to extract the value from
 * @param[out] value - the int value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getIntParameter(std::stringstream &stringStream, int &value)
{
	std::string item;
	if (std::getline(stringStream, item, ' ')) // get the item
	{
		return getInteger(item, value);
	}
	return false;
}

/**
 * @brief get an unnsigned integer value from a string stream
 * @param[in] stringStream - stream to extract the value from
 * @param[out] value - the uint value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getUintParameter(std::stringstream &stringStream, uint32_t &value)
{
	std::string item;
	if (std::getline(stringStream, item, ' ')) // get the item
	{
		return getInteger(item, value);
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////
// Script processing
//
//////////////////////////////////////////////////////////////////////
// Get a list of the urls deined in the URL test file
// This will be in the form ot 'TAG URL' e.g.:
// DASH https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd
//
/**

 * @brief Get a list of the tags and urls defined in the URL test file
 * @param[in] - the path name of the url file
 */
void ScriptedSmokeTest::getTestUrls(std::string &filename)
{
	taggedURLList.clear();

	if (filename != "")
	{
		std::ifstream urlFile(filename);
		if (urlFile.good())
		{
			std::string line;
			std::string tag;
			std::string url;
			while (std::getline(urlFile, line))
			{
				std::stringstream lineText(line);
				if (std::getline(lineText, tag, ' ') &&
					std::getline(lineText, url, ' ') &&
					tag[0] != '#')
				{
					taggedURLList[tag] = url;
				}
			}
			urlFile.close();
		}
	}
}

/**
 * @brief Check a script and create a test for each iteration specified
 * @param[in] filename - the filename of the script
 * @param[in, out] testList - the list of tests created
 * @retval true if any test iterations were added
 */
bool ScriptedSmokeTest::applyTestIterations(std::string &filename, std::vector<testInformation> &testList)
{
	bool scriptUsesTestUrls = false;
	uint32_t testsAdded = 0;

	// Open the script and check for an instruction to perform multiple iterations
	std::ifstream script(filename);
	if (script.good())
	{
		std::string line;
		std::string param;
		while (std::getline(script, line))
		{
			std::stringstream lineStream(line);
			if (getValueParameter(lineStream, "SET_ITERATIONS", param)) // look for 'SET_ITERATIONS(iterations)'
			{
				uint32_t iterations = 0;
				if (getInteger(param, iterations))
				{
					// For each iteration check for a name and append a test to the list
					for (int iter = 1; iter <= iterations; iter++)
					{
						testInformation test(filename);
						std::getline(lineStream, test.name, ' ');
						test.iteration = iter;

						testList.push_back(test);
						testsAdded++;
					}
				}
				break;
			}
		}
		
		script.close();
	}

	return testsAdded > 0;
}

/**
 * @brief Check a script for variable tags that change per iteration
 * @param[in] script - a file stream script to check
 * @param[in] iteration - the current iteration of the script (0 = no iterations, run once)
 * @param[in, out] tagMap - a map of variable name to tag value (for this iteration)
 * @retval true if no errors
 */
bool ScriptedSmokeTest::getIterationInfo(std::ifstream &script, int iteration)
{
	std::stringstream text;
	std::string line;

	text << script.rdbuf();
	script.seekg(0);

	tagMap.clear();
	while (std::getline(text, line))
	{
		std::string var;
		std::string tag;
		std::stringstream lineStream(line);
		if (getValueParameter(lineStream, "SET_VAR_TAGS", var)) // look for 'SET_VAR_TAGS(var)'
		{
			// We always expect at least one tag
			if (!std::getline(lineStream, tag, ' '))
			{
				printf("%s:%d: ERROR: Invalid 'SET_VAR_TAGS(%s)'\n",__FUNCTION__,__LINE__, var.c_str());
				return false;
			}

			// Try to extract a tag for each iteration
			for (int iter = 1; iter < iteration; iter++)
			{
				if (!std::getline(lineStream, tag, ' '))
				{
					printf("%s:%d: WARNING: too few tags in 'SET_VAR_TAGS(%s)'\n",__FUNCTION__,__LINE__, var.c_str());
				}
			}
			tagMap[var] = tag;
		}
	}
	return true;
}

/**
 * @brief Get a list of all the tests to run
 * @param[in] testScript - set if only one particular test is to be run
 * @retval a list of all the tests to run
 */
std::vector<testInformation> ScriptedSmokeTest::getTestInformation(const char *testScript)
{
	// Try to find the dir to look for test scripts
	// Try /opt/smoketest first and if that doesn't exist, try ~/smoketest
	std::string dirname("/opt/smoketest");
	std::vector<testInformation> testList;

	DIR *directory = opendir(dirname.c_str());
	if (!directory)
	{
		const char *homedir = getenv("HOME");
		if (homedir)
		{
			dirname = homedir;
			dirname += "/smoketest";
			directory = opendir(dirname.c_str());
		}
	}

	if (directory != NULL)
	{
		// Parse the test URL file
		std::string testUrlsFile = dirname + "/" + SCRIPT_TEST_URL_FILE;
		getTestUrls(testUrlsFile);

		// Check all the files lloking for '.smk' scripts
		struct dirent *file = readdir(directory);
		while (file)
		{
			std::string filename(file->d_name);
			if (filename.size() > 4)
			{
				if (filename.substr(filename.size() - 4) == ".smk")
				{
					std::string filepath = dirname + "/" + filename;

					if ((testScript != NULL) &&
					    (filename != testScript))
					{
						// Skip this as we have been given a speciific file to run and it doesn't match
					}
					// Check for requested iterations and create tests
					else if (!applyTestIterations(filepath, testList))
					{
						// Didn't find any iterations specified so just add the test
						testList.push_back(testInformation(filepath));
					}
				}
			}

			file = readdir(directory);
		}
	}

    return testList;
}

/**
 * @brief Run the specified script
 * @param[in] filename - the script to run
 * @param[in] iteration - the iteration of the script (0 = no iterations specified)
 * @param[in] gtest - google tests enabled
 * @retval true if the script ran successfully
 */
bool ScriptedSmokeTest::runScript(const char *filename, int iteration, bool gtest)
{
	std::ifstream script(filename);
	std::stringstream text;
	std::string line;
	std::stringstream::pos_type loopstart;
	uint32_t loopcount = 0;
	bool scriptOk = true;
	int lineNumber = 0;

	printf("%s:%d: Run script %s...\n",__FUNCTION__,__LINE__, filename);

	if (script.good())
	{
		if (!getIterationInfo(script, iteration))
		{
		}

		text << script.rdbuf();
		script.close();
		
		currentPlayerId = 0;
		while(std::getline(text, line) && scriptOk) 
		{
			PlayerInstanceAAMP *playerInstance = NULL;
			ScriptedSmokeTestEventListener *listener = NULL;
			std::stringstream cmd(line);
			std::string token;
			lineNumber++;

			printf("%s:%d: >>> %s\n",__FUNCTION__,__LINE__, cmd.str().c_str());

			if ((line.length() > 0) && (line[0] == '#'))
			{
				// comment line
			}
			else if (std::getline(cmd, token, ' '))
			{
				// Do any select first then get the player and listener
				if (token == "select")
				{
					if (!getUintParameter(cmd, currentPlayerId))
					{
						printf("%s:%d: Failed to get player id\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Select player %d\n",__FUNCTION__,__LINE__, currentPlayerId);

						// Check the selected player is valid
						if (!getCurrentPlayer() || !getCurrentListener())
						{
							printf("%s:%d: Invalid player id (%d)\n",__FUNCTION__,__LINE__, currentPlayerId);
							scriptOk = false;
						}
					}

					continue;
				}

				// Get the active player and listener
				playerInstance = getCurrentPlayer();
				if (playerInstance == NULL)
				{
					printf("%s:%d: Invalid player %d\n",__FUNCTION__,__LINE__, currentPlayerId);
					scriptOk = false;
					break;
				}
				listener = getCurrentListener();
				if (listener == NULL)
				{
					printf("%s:%d: Invalid listener %d\n",__FUNCTION__,__LINE__, currentPlayerId);
					scriptOk = false;
					break;
				}

				if (token == "new") // create a new player
				{
					currentPlayerId = mAampPlayer.newPlayer();
					printf("%s:%d: New player created and selected %d\n",__FUNCTION__,__LINE__, currentPlayerId);
				}
				else if (token == "loopstart") // start of a loop
				{
					if (loopcount >= 0) // currently looping?
					{
						printf("%s:%d: Invalid loop start\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (!getUintParameter(cmd, loopcount))
					{
						printf("%s:%d: Failed to get loop count\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (loopcount <= 0)
					{
						printf("%s:%d: Invalid loop count (%d)\n",__FUNCTION__,__LINE__, loopcount);
						scriptOk = false;
					}
					else
					{
						loopstart = text.tellg();
					}
				}
				else if (token == "loopend") // end of loop
				{
					if (loopcount < 0)
					{
						printf("%s:%d: Invalid loop end\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (--loopcount)
					{
						text.seekg(loopstart);
					}
					else
					{
						printf("%s:%d: Loop complete\n",__FUNCTION__,__LINE__);
						loopcount = -1;
					}
				}
				else if (token == "waitfor") // check for specified events
				{
					std::string message;
	                bool receivedEvents = listener->WaitForEvent(cmd, message);
					if (!receivedEvents)
					{
						if (gtest)
						{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
							EXPECT_TRUE(receivedEvents) << "line " << std::to_string(lineNumber) << ": " << message;
						}
						else
						{
							printf("\n>>>>>>>>>>>>>>>>\n%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
						}

						scriptOk = false;
					}
				}
				else if (token == "failon") // set any events to fail script on
				{
					std::string message;
	                if (!listener->SetFailEvents(cmd))
					{
						printf("%s:%d: Failed to get set fail events\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
				}
				else if (token == "sleep") // sleep for 'n' seconds
				{
					uint32_t seconds = 0;
					if (!getUintParameter(cmd, seconds))
					{
						printf("%s:%d: Failed to get seconds\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Sleep for %d...\n",__FUNCTION__,__LINE__, seconds);
						sleep(seconds);
					}
				}
				else if ((token.substr(0, 12) == "SET_VAR_TAGS") ||
						 (token.substr(0, 14) == "SET_ITERATIONS"))
				{
					// ignore
				}

				
				//
				// AAAMP command processing
				//
				else if (token == "tune")
				{
					scriptOk = process_tune(cmd, playerInstance);
				}
				else if (token == "stop")
				{
					scriptOk = process_stop(cmd, playerInstance);
				}
				else if (token == "detach")
				{
					scriptOk = process_detach(cmd, playerInstance);
					mAampPlayer.deletePlayer(currentPlayerId);
					currentPlayerId = currentPlayerId ? currentPlayerId-- : currentPlayerId++;
				}
				else if (token == "setrate")
				{
					scriptOk = process_setrate(cmd, playerInstance);
				}
				else if (token == "ff")
				{
					scriptOk = process_ff(cmd, playerInstance);
				}
				else if (token == "rew")
				{
					scriptOk = process_rew(cmd, playerInstance);
				}
				else if (token == "pause")
				{
					scriptOk = process_pause(cmd, playerInstance);
				}
				else if (token == "play")
				{
					scriptOk = process_play(cmd, playerInstance);
				}
				else if (token == "seek")
				{
					scriptOk = process_seek(cmd, playerInstance);
				}
				else if (token == "async")
				{
					scriptOk = process_async(cmd, playerInstance);
				}
				else if (token == "config")
				{
					scriptOk = process_config(cmd, playerInstance);
				}
				else if (token == "setaudiotrack")
				{
					scriptOk = process_setaudiotrack(cmd, playerInstance);
				}
				else if (token == "settextrack")
				{
					scriptOk = process_settextrack(cmd, playerInstance);
				}
				else
				{
					printf("%s:%d: ERROR: unrecognised command '%s'\n",__FUNCTION__,__LINE__, token.c_str());
					scriptOk = false;
				}
			}

			if ((listener != NULL) && scriptOk)
			{
				std::string message;
				bool checkFailEvents = listener->CheckFailEvents(message);
				if (!checkFailEvents)
				{
					if (gtest)
					{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
						EXPECT_TRUE(checkFailEvents) << message;
					}
					else
					{
						printf("\n>>>>>>>>>>>>>>>>\n%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
					}

					scriptOk = false;
				}
			}
		}
	}
	else
	{
		printf("%s:%d: Failed to open script\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}

	// Delete player instances that have been created
	mAampPlayer.resetPlayers();

	printf("%s:%d: \n>>> Script %s\n\n",__FUNCTION__,__LINE__, scriptOk?"SUCCEEDED":"FAILED");
	if (!scriptOk)
	{
		//TBD - copy aamp log to an error log
		//std::string errlog = filename;
	}

	return scriptOk;
}


/**
 * @brief process_tune
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_tune(std::stringstream &args, PlayerInstanceAAMP *player)
{
	std::string url;
	uint32_t autoplay = 1;
	bool scriptOk = true;

	if (!std::getline(args, url, ' '))
	{
		printf("%s:%d: Failed to get url\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else if (!getUintParameter(args, autoplay))
	{
		printf("%s:%d: Failed to get autoplay, default to 1\n",__FUNCTION__,__LINE__);
		autoplay = 1;
	}

	// if url begins with '$' then assume it is some sort of environment variable
	std::string var;
	if (getEnvVarValue(url, var))
	{
		// First see if it is an exported variable
		if (getEnvVar(var, url))
		{
			printf("%s:%d: Got url from environment: '%s'\n",__FUNCTION__,__LINE__, url.c_str());
		}
		else
		{
			// See if it has been defined in the tag map (via SET_VAR_TAGS(name))
			std::map<std::string, std::string>::iterator varIter = tagMap.find(var);
			if (varIter != tagMap.end())
			{
				std::string tag = varIter->second;

				// See if the tag exists in the tagged URL list
				std::map<std::string, std::string>::iterator tagIter = taggedURLList.find(tag);
				if (tagIter != taggedURLList.end())
				{
					// Found it so we'll set the URL from the tagged URL entry
					printf("%s:%d: Got url from var (%s -> %s -> %s)\n",__FUNCTION__,__LINE__, 
						var.c_str(), tag.c_str(), url.c_str());
					url = tagIter->second;
				}
				else
				{
					// We failed to find it so we'll just try to use the 'tag' as the URL
					printf("%s:%d: Unable to find the tag '%s' so using it as url\n",__FUNCTION__,__LINE__, tag.c_str());
					url = tag;
				}
			}
			else
			{
				printf("%s:%d: Failed to resolve environment variable '%s'\n",__FUNCTION__,__LINE__, url.c_str());
				scriptOk = false;
			}
		}
	}

	if (scriptOk)
	{
		printf("%s:%d: Tune to '%s'\n",__FUNCTION__,__LINE__, url.c_str());
		player->Tune(url.c_str(), autoplay);
	}

	return scriptOk;
}

/**
 * @brief process_stop
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_stop(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Stop\n",__FUNCTION__,__LINE__);
	player->Stop();
	return true;
}

/**
 * @brief process_detach
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_detach(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Detach\n",__FUNCTION__,__LINE__);
	player->detach();
	return true;
}

/**
 * @brief process_setrate
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_setrate(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	int rate = 1;
	if (!getIntParameter(args, rate))
	{
		printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: SetRate(%d)\n",__FUNCTION__,__LINE__, rate);
		player->SetRate(rate);
	}
	return scriptOk;
}

/**
 * @brief process_ff
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_ff(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t rate = 1;
	if (!getUintParameter(args, rate))
	{
		printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: ff(%d)\n",__FUNCTION__,__LINE__, rate);
		player->SetRate(rate);
	}
	return scriptOk;
}

/**
 * @brief process_rew
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_rew(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t rate = 1;
	if (!getUintParameter(args, rate))
	{
		printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: rew(%d)\n",__FUNCTION__,__LINE__, rate);
		player->SetRate(rate);
	}
	return scriptOk;
}

/**
 * @brief process_pause
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_pause(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Pause\n",__FUNCTION__,__LINE__);
	player->SetRate(0);
	return true;
}

/**
 * @brief process_play
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_play(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Play\n",__FUNCTION__,__LINE__);
	player->SetRate(1);
	return true;
}

/**
 * @brief process_seek
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_seek(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	std::string param;
	uint32_t position = 0;
	uint32_t keepPaused = 0;
	std::string name;
	std::string value;

	if (!std::getline(args, param, ' '))
	{
		printf("%s:%d: Failed to get position param\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else if (getEnvVarValue(param, name, value)) // could be 'name(value)'
	{
		// First see if 'name' is an exported variable
		std::string exportedValue;
		if (getEnvVar(name, exportedValue))
		{
			// The parameter was an exported environment valiable so use it's value
			scriptOk = getInteger(exportedValue, position);
		}
		else if (name == "DURATION")
		{
			// Special case 'DURATION' - use the asset duration
			ScriptedSmokeTestEventListener *listener = getCurrentListener();
			if (listener)
			{
				position = (listener->GetDuration() / 1000);
			}
			else
			{
				printf("%s:%d: ERROR accessing event listener\n",__FUNCTION__,__LINE__);
				scriptOk = false;
			}
		}
		else
		{
			printf("%s:%d: Unrecognised environment variable '%s'\n",__FUNCTION__,__LINE__, param.c_str());
			scriptOk = false;
		}

		if (scriptOk && !value.empty())
		{
			int adjust = 0;
			scriptOk = getInteger(value, adjust);
			position += adjust;
		}
	}
	else if (!getInteger(param, position))
	{
		printf("%s:%d: Failed to get position from '%s'\n",__FUNCTION__,__LINE__, param.c_str());
		scriptOk = false;
	}

	if (scriptOk)
	{
		if (!getUintParameter(args, keepPaused))
		{
			printf("%s:%d: Failed to get keepPaused, default to 0\n",__FUNCTION__,__LINE__);
			keepPaused = 0;
		}

		printf("%s:%d: Seek(%d)\n",__FUNCTION__,__LINE__, position);
		player->Seek(position, keepPaused);
	}

	return scriptOk;
}

/**
 * @brief process_async
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_async(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t asyncEnabled = 0;
	if (!getUintParameter(args, asyncEnabled))
	{
		printf("%s:%d: Failed to get async enabled\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: Sleep for %d...\n",__FUNCTION__,__LINE__, asyncEnabled);
		player->SetAsyncTuneConfig(asyncEnabled);
	}
	return scriptOk;
}

/**
 * @brief process_config
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_config(std::stringstream &args, PlayerInstanceAAMP *player)
{
	std::string quotes = "\"";
	std::string comma = ",";
	std::string colon = ":";

	std::string config = "{";
	std::string name;
	std::string value;
	int parameterCount = 0;
	while (getValueParameter(args, name, value))
	{
		if (parameterCount)
		{
			config += comma;
		}

		config += quotes + name + quotes + colon;
		if (getIsNumber(value) || 
			(value == "true") || 
			(value == "false"))
		{
			config += value;
		}
		else
		{
			config += quotes + value + quotes;
		}
		parameterCount++;
	}
	config += "}";

	if (config.size() > 2)
	{
		printf("%s:%d: Set config %s...\n",__FUNCTION__,__LINE__, config.c_str());
		player->InitAAMPConfig((char *)config.c_str());
	}

	return true;
}

/**
 * @brief process_setaudiotrack
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_setaudiotrack(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t track = 0;
	if (!getUintParameter(args, track))
	{
		printf("%s:%d: Failed to get audio track\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: Set audio track %d...\n",__FUNCTION__,__LINE__, track);
		player->SetAudioTrack(track);
	}
	return scriptOk;
}

/**
 * @brief process_settextrack
 * @param[in] args - stream of arguments
 * @param[in] player - the aamp player instance
 * @retval true if command was parsed and sent successfully
 */
bool ScriptedSmokeTest::process_settextrack(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t track = 0;
	if (!getUintParameter(args, track))
	{
		printf("%s:%d: Failed to get text track\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else
	{
		printf("%s:%d: Set text track %d...\n",__FUNCTION__,__LINE__, track);
		player->SetTextTrack(track);
	}
	return scriptOk;
}










//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST running of scripts
//
// This code will generate a number of tests - at least one for each script (but a script
// may have multiple iterations)
//

class ScriptTest : public ::testing::TestWithParam<testInformation> {
  // You can implement all the usual fixture class members here.
  // To access the test parameter, call GetParam() from class
  // TestWithParam<T>.
public:
	ScriptTest(): _scriptInfo()
	{
	}

	void SetUp() override 
	{
		_scriptInfo = GetParam();
	}
	void TearDown() override
	{
	}

	bool runScript(const char *scriptFilename, int iteration)
	{
		return ScriptedSmokeTest::runScript(scriptFilename, iteration);
	}

protected:
	testInformation _scriptInfo;
};

TEST_P(ScriptTest, RunScriptTest)
{
	EXPECT_TRUE(runScript(_scriptInfo.file.c_str(), _scriptInfo.iteration)) << _scriptInfo.name;
}

// Create a test for each element in the vector returned by ScriptedSmokeTest::getTestInformation()
INSTANTIATE_TEST_SUITE_P(SmokeTestScripts, ScriptTest, ::testing::ValuesIn(ScriptedSmokeTest::getTestInformation()));






#if 0
bool ScriptedSmokeTest::runScript(const char *filename, int iteration, bool gtest)
{
	std::ifstream script(filename);
	std::stringstream text;
	std::string line;
	std::stringstream::pos_type loopstart;
	uint32_t loopcount = 0;
	bool scriptOk = true;
	int lineNumber = 0;
	uint32_t playerId = 0;

	printf("%s:%d: Run script %s...\n",__FUNCTION__,__LINE__, filename);

	if (script.good())
	{
		if (!getIterationInfo(script, iteration))
		{
		}

		text << script.rdbuf();
		script.close();
		
		while(std::getline(text, line) && scriptOk) 
		{
			PlayerInstanceAAMP *playerInstance = NULL;
			ScriptedSmokeTestEventListener *listener = NULL;
			std::stringstream cmd(line);
			std::string token;
			lineNumber++;

			printf("%s:%d: >>> %s\n",__FUNCTION__,__LINE__, cmd.str().c_str());

			if ((line.length() > 0) && (line[0] == '#'))
			{
				// comment line
			}
			else if (std::getline(cmd, token, ' '))
			{
				if (token == "select")
				{
					if (!getUintParameter(cmd, playerId))
					{
						printf("%s:%d: Failed to get player id\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Select player %d\n",__FUNCTION__,__LINE__, playerId);

						// Check the selected player is valid
						if (!mAampPlayer.getPlayer(playerId) || !mAampPlayer.getListener(playerId))
						{
							printf("%s:%d: Invalid player id (%d)\n",__FUNCTION__,__LINE__, playerId);
							scriptOk = false;
						}
					}

					continue;
				}

				// Get the active player and listener
				playerInstance = mAampPlayer.getPlayer(playerId);
				if (playerInstance == NULL)
				{
					printf("%s:%d: Invalid player %d\n",__FUNCTION__,__LINE__, playerId);
					scriptOk = false;
					break;
				}
				listener = dynamic_cast<ScriptedSmokeTestEventListener*>(mAampPlayer.getListener(playerId));
				if (listener == NULL)
				{
					printf("%s:%d: Invalid listener %d\n",__FUNCTION__,__LINE__, playerId);
					scriptOk = false;
					break;
				}

				if (token == "new")
				{
					playerId = mAampPlayer.newPlayer();
					printf("%s:%d: New player created and selected %d\n",__FUNCTION__,__LINE__, playerId);
				}
				else if (token == "loopstart")
				{
					if (loopcount >= 0)
					{
						printf("%s:%d: Invalid loop start\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (!getUintParameter(cmd, loopcount))
					{
						printf("%s:%d: Failed to get loop count\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (loopcount <= 0)
					{
						printf("%s:%d: Invalid loop count (%d)\n",__FUNCTION__,__LINE__, loopcount);
						scriptOk = false;
					}
					else
					{
						loopstart = text.tellg();
					}
				}
				else if (token == "loopend")
				{
					if (loopcount < 0)
					{
						printf("%s:%d: Invalid loop end\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (--loopcount)
					{
						text.seekg(loopstart);
					}
					else
					{
						printf("%s:%d: Loop complete\n",__FUNCTION__,__LINE__);
						loopcount = -1;
					}
				}
				else if (token == "tune")
				{
					std::string url;
					uint32_t autoplay = 1;

					if (!std::getline(cmd, url, ' '))
					{
						printf("%s:%d: Failed to get url\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (!getUintParameter(cmd, autoplay))
					{
						printf("%s:%d: Failed to get autoplay, default to 1\n",__FUNCTION__,__LINE__);
						autoplay = 1;
					}

					// if url begins with '$' then assume it is some sort of environment variable
					std::string var;
					if (getEnvVarValue(url, var))
					{
						// First see if it is an exported variable
						if (getEnvVar(var, url))
						{
							printf("%s:%d: Got url from environment: '%s'\n",__FUNCTION__,__LINE__, url.c_str());
						}
						else
						{
							// See if it has been defined in the tag map (via SET_VAR_TAGS(name))
							std::map<std::string, std::string>::iterator varIter = tagMap.find(var);
							if (varIter != tagMap.end())
							{
								std::string tag = varIter->second;

								// See if the tag exists in the tagged URL list
								std::map<std::string, std::string>::iterator tagIter = taggedURLList.find(tag);
								if (tagIter != taggedURLList.end())
								{
									// Found it so we'll set the URL from the tagged URL entry
									printf("%s:%d: Got url from var (%s -> %s -> %s)\n",__FUNCTION__,__LINE__, 
										var.c_str(), tag.c_str(), url.c_str());
									url = tagIter->second;
								}
								else
								{
									// We failed to find it so we'll just try to use the 'tag' as the URL
									printf("%s:%d: Unable to find the tag '%s' so using it as url\n",__FUNCTION__,__LINE__, tag.c_str());
									url = tag;
								}
							}
							else
							{
								printf("%s:%d: Failed to resolve environment variable '%s'\n",__FUNCTION__,__LINE__, url.c_str());
								scriptOk = false;
							}
						}
					}

					if (scriptOk)
					{
						printf("%s:%d: Tune to '%s'\n",__FUNCTION__,__LINE__, url.c_str());
						playerInstance->Tune(url.c_str(), autoplay);
					}
				}
				else if (token == "stop")
				{
					printf("%s:%d: Stop\n",__FUNCTION__,__LINE__);
					playerInstance->Stop();
				}
				else if (token == "detach")
				{
					printf("%s:%d: Detach\n",__FUNCTION__,__LINE__);
					playerInstance->detach();
					mAampPlayer.deletePlayer(playerId);
					playerId = playerId ? playerId-- : playerId++;
				}
				else if (token == "setrate")
				{
					int rate = 1;
					if (!getIntParameter(cmd, rate))
					{
						printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: SetRate(%d)\n",__FUNCTION__,__LINE__, rate);
						playerInstance->SetRate(rate);
					}
				}
				else if (token == "ff")
				{
					uint32_t rate = 1;
					if (!getUintParameter(cmd, rate))
					{
						printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: ff(%d)\n",__FUNCTION__,__LINE__, rate);
						playerInstance->SetRate(rate);
					}
				}
				else if (token == "rew")
				{
					uint32_t rate = 1;
					if (!getUintParameter(cmd, rate))
					{
						printf("%s:%d: Failed to get rate\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: rew(%d)\n",__FUNCTION__,__LINE__, rate);
						playerInstance->SetRate(rate);
					}
				}
				else if (token == "pause")
				{
					printf("%s:%d: Pause\n",__FUNCTION__,__LINE__);
					playerInstance->SetRate(0);
				}
				else if (token == "play")
				{
					printf("%s:%d: Play\n",__FUNCTION__,__LINE__);
					playerInstance->SetRate(1);
				}
				else if (token == "seek")
				{
					std::string param;
					uint32_t position = 0;
					uint32_t keepPaused = 0;
					std::string name;
					std::string value;

					if (!std::getline(cmd, param, ' '))
					{
						printf("%s:%d: Failed to get position param\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else if (getEnvVarValue(param, name, value)) // could be 'name(value)'
					{
						// First see if 'name' is an exported variable
						std::string exportedValue;
						if (getEnvVar(name, exportedValue))
						{
							// The parameter was an exported environment valiable so use it's value
							scriptOk = getInteger(exportedValue, position);
						}
						else if (name == "DURATION")
						{
							// Special case 'DURATION' - use the asset duration
							position = (listener->GetDuration() / 1000);
						}
						else
						{
							printf("%s:%d: Unrecognised environment variable '%s'\n",__FUNCTION__,__LINE__, param.c_str());
							scriptOk = false;
						}

						if (scriptOk && !value.empty())
						{
							int adjust = 0;
							scriptOk = getInteger(value, adjust);
							position += adjust;
						}
					}
					else if (!getInteger(param, position))
					{
						printf("%s:%d: Failed to get position from '%s'\n",__FUNCTION__,__LINE__, param.c_str());
						scriptOk = false;
					}

					if (scriptOk)
					{
						if (!getUintParameter(cmd, keepPaused))
						{
							printf("%s:%d: Failed to get keepPaused, default to 0\n",__FUNCTION__,__LINE__);
							keepPaused = 0;
						}

						printf("%s:%d: Seek(%d)\n",__FUNCTION__,__LINE__, position);
						playerInstance->Seek(position, keepPaused);
					}
				}
				else if (token == "waitfor")
				{
					std::string message;
	                bool receivedEvents = listener->WaitForEvent(cmd, message);
					if (!receivedEvents)
					{
						if (gtest)
						{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
							EXPECT_TRUE(receivedEvents) << "line " << std::to_string(lineNumber) << ": " << message;
						}
						else
						{
							printf("\n>>>>>>>>>>>>>>>>\n%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
						}

						scriptOk = false;
					}
				}
				else if (token == "failon")
				{
					std::string message;
	                if (!listener->SetFailEvents(cmd))
					{
						printf("%s:%d: Failed to get set fail events\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
				}
				else if (token == "sleep")
				{
					uint32_t seconds = 0;
					if (!getUintParameter(cmd, seconds))
					{
						printf("%s:%d: Failed to get seconds\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Sleep for %d...\n",__FUNCTION__,__LINE__, seconds);
						sleep(seconds);
					}
				}
				else if (token == "async")
				{
					uint32_t asyncEnabled = 0;
					if (!getUintParameter(cmd, asyncEnabled))
					{
						printf("%s:%d: Failed to get async enabled\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Sleep for %d...\n",__FUNCTION__,__LINE__, asyncEnabled);
						playerInstance->SetAsyncTuneConfig(asyncEnabled);
					}
				}
				else if (token == "config")
				{
					std::string quotes = "\"";
					std::string comma = ",";
					std::string colon = ":";

					std::string config = "{";
					std::string name;
					std::string value;
					int parameterCount = 0;
					while (getValueParameter(cmd, name, value))
					{
						if (parameterCount)
						{
							config += comma;
						}

						config += quotes + name + quotes + colon;
						if (getIsNumber(value) || 
						    (value == "true") || 
							(value == "false"))
						{
							config += value;
						}
						else
						{
							config += quotes + value + quotes;
						}
						parameterCount++;
					}
					config += "}";

					if (config.size() > 2)
					{
						printf("%s:%d: Set config %s...\n",__FUNCTION__,__LINE__, config.c_str());
						playerInstance->InitAAMPConfig((char *)config.c_str());
					}
				}
				else if (token == "setaudiotrack")
				{
					uint32_t track = 0;
					if (!getUintParameter(cmd, track))
					{
						printf("%s:%d: Failed to get audio track\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Set audio track %d...\n",__FUNCTION__,__LINE__, track);
						playerInstance->SetAudioTrack(track);
					}
				}
				else if (token == "settextrack")
				{
					uint32_t track = 0;
					if (!getUintParameter(cmd, track))
					{
						printf("%s:%d: Failed to get text track\n",__FUNCTION__,__LINE__);
						scriptOk = false;
					}
					else
					{
						printf("%s:%d: Set text track %d...\n",__FUNCTION__,__LINE__, track);
						playerInstance->SetTextTrack(track);
					}
				}
				else if ((token.substr(0, 12) == "SET_VAR_TAGS") ||
						 (token.substr(0, 14) == "SET_ITERATIONS"))
				{
					// ignore
				}
				else
				{
					printf("%s:%d: ERROR: unrecognised command '%s'\n",__FUNCTION__,__LINE__, token.c_str());
					scriptOk = false;
				}
			}

			if ((listener != NULL) && scriptOk)
			{
				std::string message;
				bool checkFailEvents = listener->CheckFailEvents(message);
				if (!checkFailEvents)
				{
					if (gtest)
					{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
						EXPECT_TRUE(checkFailEvents) << message;
					}
					else
					{
						printf("\n>>>>>>>>>>>>>>>>\n%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
					}

					scriptOk = false;
				}
			}
		}
	}
	else
	{
		printf("%s:%d: Failed to open script\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}

	// Delete player instances that have been created
	mAampPlayer.resetPlayers();

	printf("%s:%d: \n>>> Script %s\n\n",__FUNCTION__,__LINE__, scriptOk?"SUCCEEDED":"FAILED");
	if (!scriptOk)
	{
		//TBD - copy aamp log to an error log
		//std::string errlog = filename;
	}

	return scriptOk;
}
#endif