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
std::string ScriptedSmokeTest::smoketestDirectory;
std::string ScriptedSmokeTest::scriptDirectory;
bool ScriptedSmokeTest::abortTests = false;

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
	std::string name; 		// the name of the test (iteration) specified in the script
	int iteration;
};

const bool operator==(const testInformation& lhs, const std::string &rhs)
{
    return lhs.name == rhs;
}

const bool operator<(const testInformation& lhs, const testInformation &rhs)
{
		int test = lhs.file.compare(rhs.file);
		if (test < 0)
		{
			return true;
		}
		else if (test > 0)
		{
			return false;
		}
		return lhs.iteration < rhs.iteration;
}

std::ostream& operator<<(std::ostream& os, const testInformation& info)
{
	std::string filename = info.file.substr(0, info.file.find('.')); // remove '.smk'

	if (info.name != "")
	{
		os << filename << "_" << info.name;
	}
	else if (info.iteration)
	{
		os << filename << "_" << info.iteration;
	}
	else
	{
		os << filename;
	}
	return os;
}



/**
 * @brief Get the currently selected aamp player instance
 * @retval pointer to aamp player instance
 */
PlayerInstanceAAMP *ScriptedSmokeTest::getCurrentPlayer()
{
	return mAampPlayer.getPlayer(currentPlayerId);
}

/**
 * @brief Get the currently selected aamp player event listener
 * @retval pointer to event listener
 */
ScriptedSmokeTestEventListener *ScriptedSmokeTest::getCurrentListener()
{
	return dynamic_cast<ScriptedSmokeTestEventListener*>(mAampPlayer.getListener(currentPlayerId));
}

bool ScriptedSmokeTest::isFile(const char *path, bool directory)
{
	if (NULL != path)
	{
		struct stat fileinfo;	
		if (stat(path, &fileinfo) == 0)
		{
			if (directory)
			{
				return ((fileinfo.st_mode & S_IFDIR) == S_IFDIR);
			}
			else
			{
				return ((fileinfo.st_mode & S_IFREG) == S_IFREG);
			}
		}
	}
	return false;
}

/**
 * @brief Check the scrip to se if it is a smoke test script.
 *        If it is then set it as the only script to test.
 * @param[in] script - the filename of the test to run
 * @retval true if it was recognised as a smoke test script
 */
bool ScriptedSmokeTest::setTestScript(const char *script)
{
	std::string filepath(script);
	if (filepath.find(".smk") != std::string::npos) // if it contains '.smk' then it is a smoke test script
	{
		scriptDirectory = filepath; // set the scriptDirectory to point to the file
		return true;
	}
	return false;
}


/**
 * @brief Get the smoketest directory
 * @retval true if the directory was found
 */
bool ScriptedSmokeTest::getScriptDirectory()
{
	// Try to find the dir to look for test scripts

	// Try /opt/smoketest
	smoketestDirectory = "/opt/smoketest";

	if (!isFile(smoketestDirectory.c_str(), true))
	{
		// Try ~/smoketest
		const char *homedir = getenv("HOME");
		if (homedir)
		{
			smoketestDirectory = homedir;
			smoketestDirectory += "/smoketest";
		}
	}

	if (!isFile(smoketestDirectory.c_str(), true))
	{
		// Just create a dir in /tmp
		smoketestDirectory = "/tmp/smoketest";
		std::system("mkdir -p /tmp/smoketest");
		printf("WARNING: unable to find smoketest dir, using '/tmp/smoketest'");
	}

	if (scriptDirectory.empty())
	{
		scriptDirectory = smoketestDirectory;
	}

	return isFile(smoketestDirectory.c_str(), true);
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
 * @brief check for envornment variable - '$name' and strip the '$'
 * @param[in out] param - string parameter
 * @retval true if the param was an environment variable (starts with '$')
 */
bool ScriptedSmokeTest::getEnvironmentVariableName(std::string &param)
{
	if (param[0] == '$')
	{
		param.erase(0, 1);
		return true;
	}
	return false;

}

/**
 * @brief get environment variable
 * @param[in] name - the name of the variable
 * @param[out] value - the value of the variable
 * @retval true if 'name' was an exported environment variable
 */
bool ScriptedSmokeTest::getEnvironmentVariable(std::string &name, std::string &value)
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
 * @brief get a named string value of the form 'name(value)' or 'name:value'
 * @param[in] stringStream - stream to extract the value from
 * @param[out] name - the name value extracted
 * @param[in, out] value - the string value extracted
 * @param[in] assertName - true if there must be a name(value) - use false if the name is optional
 * @param[in] assertValue - true if there must be a value - 'n(v)', false if the value part is optional - 'n'
 * @param[in] assertNotEmptyValue - true if the value must be set - 'n(v)', false if it can be empty - 'n()'
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::stringstream &stringStream, std::string &name, std::string &value, 
                                          bool assertName, bool assertValue, bool assertNotEmptyValue)
{
	enum parsingStage {	NAME, VALUE, ERROR };
	parsingStage stage = NAME;

	name.clear();
	value.clear();

	if (!stringStream.eof())
	{
		char c;
		int brackets = 0;
		while (stringStream.get(c))
		{
			if (c == '(')
			{
				if (name.empty())
				{
					printf("%s:%d: ERROR: Syntax error: '%s:%s'\n",__FUNCTION__,__LINE__, name.c_str(), value.c_str());
					return false;
				}
				else if (!brackets && (stage == VALUE))
				{
					printf("%s:%d: ERROR: Syntax error: '%s:%s'\n",__FUNCTION__,__LINE__, name.c_str(), value.c_str());
					return false;
				}
				else if (++brackets == 1)
				{
					stage = VALUE;
					continue;
				}
			}
			else if (c == ')')
			{
				if (!brackets)
				{
					printf("%s:%d: ERROR: Syntax error: '%s:%s'\n",__FUNCTION__,__LINE__, name.c_str(), value.c_str());
					return false;
				}
				else if (--brackets == 0)
				{
					continue;
				}
			}
			else if (c == ':')
			{
				if (name.empty())
				{
					printf("%s:%d: ERROR: Syntax error: '%s:%s'\n",__FUNCTION__,__LINE__, name.c_str(), value.c_str());
					return false;
				}
				else if ((stage == NAME) &&
				         (name.substr(0,4) != "http"))
				{
					stage = VALUE;
					continue;
				}
			}
			else if (c == ',')
			{
				if (!brackets)
				{
					break;
				}
			}
			else if (c == ' ')
			{
				if (!brackets && !name.empty())
				{
					break;
				}
				continue; // Strip spaces - don't add them to the result
			}
			
			if (stage == NAME)
			{
				name += c;
			}
			else if (stage == VALUE)
			{
				value += c;
			}
			else
			{
				printf("%s:%d: ERROR: Syntax error: '%s:%s'\n",__FUNCTION__,__LINE__, name.c_str(), value.c_str());
				return false;
			}
		}

		if (name.empty())
		{
			return !assertName;
		}

		if (value.empty())
		{
			if (stage == VALUE)
			{
				return !assertNotEmptyValue;
			}
			else
			{
				return !assertValue;
			}
		}

		return true;
	}
	return false;
}

/**
 * @brief get a named string value of the form 'name(value)'
 * @param[in] item - stream to extract the value from
 * @param[in, out] name - the name value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::string &item, std::string &name, std::string &value)
{
	std::stringstream stream(item);
	return getValueParameter(stream, name, value);
}

/**
 * @brief get a value from a string list
 * @param[in] item - stream to extract the value from
 * @param[in, out] value - the value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::getValueParameter(std::stringstream &item, std::string &name, bool assertName)
{
	std::string value;
	return getValueParameter(item, name, value, assertName, false);
}

/**
 * @brief get a named string value of the form 'name(value)' with a specified name
 * @param[in] stringStream - stream to extract the value from
 * @param[in] name - the name to check for
 * @param[out] value - the string value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::checkForValueParameter(std::stringstream &stringStream, const char *name, std::string &value)
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
 * @brief get a named string value of the form 'name(value)' with a specified name
 * @param[in] line - string to extract the value from
 * @param[in] name - the name to check for
 * @param[out] value - the string value extracted
 * @retval true if no error occured
 */
bool ScriptedSmokeTest::checkForValueParameter(std::string &line, const char *name, std::string &value)
{
	std::stringstream stringStream(line);
	return checkForValueParameter(stringStream, name, value);
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
	if (getValueParameter(stringStream, item)) // get the item
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
	if (getValueParameter(stringStream, item)) // get the item
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
				if (getValueParameter(lineText, tag) &&
					getValueParameter(lineText, url) &&
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
 * @brief Check a script for a list of target devices
 * @param[in] filename - the filename of the script
 * @retval true no specified targets or targets matched current device
 */
bool ScriptedSmokeTest::checkTargetDevice(std::string &filename)
{
	bool targetSpecified = false;

	// Open the script and check for an instruction to target only specified device(s)
	std::ifstream script(getScriptFilePath(filename.c_str()));
	if (script.good())
	{
		std::string device;
#ifdef SIMULATOR_BUILD
		device = "simulator";
#else
		std::string deviceName = getenv("DEVICE_NAME");
		std::string locale = getenv("GDPR_LOCALE");
		device = deviceName + locale;
#endif

		std::string line;
		std::string param;
		while (std::getline(script, line))
		{
			if (checkForValueParameter(line, "SET_TARGET", param)) // look for 'SET_TARGET(target)'
			{
				std::string target;
				std::stringstream deviceStream(param);
				while (getValueParameter(deviceStream, target)) // look through list 'dev1, dev2, dev3 ...'
				{
					// Check for NOT device
					if (target[0] == '!')
					{
						target.erase(0, 1);
						if (device == target)
						{
							printf("%s:%d: WARNING: Skipping test for device '%s' (target '%s')\n",__FUNCTION__,__LINE__, device.c_str(), target.c_str());
							return false;
						}
					}
					else
					{
						targetSpecified = true;
						
						if (device == target)
						{
							printf("%s:%d: INFO: Target device matched '%s'\n",__FUNCTION__,__LINE__, device.c_str());
							return true;
						}
					}
				}
			}
		}
	}

	return !targetSpecified; // return false if target devices were specified (but not matched)
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
	std::ifstream script(getScriptFilePath(filename.c_str()));
	if (script.good())
	{
		std::string line;
		std::string param;
		while (std::getline(script, line))
		{
			std::stringstream lineStream(line);
			if (checkForValueParameter(lineStream, "SET_ITERATIONS", param)) // look for 'SET_ITERATIONS(iterations)'
			{
				uint32_t iterations = 0;
				if (getInteger(param, iterations))
				{
					// For each iteration check for a name and append a test to the list
					for (int iter = 1; iter <= iterations; iter++)
					{
						testInformation test(filename);
						getValueParameter(lineStream, test.name);
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
		if (checkForValueParameter(lineStream, "SET_VAR_TAGS", var)) // look for 'SET_VAR_TAGS(var)'
		{
			// We always expect at least one tag
			if (!getValueParameter(lineStream, tag))
			{
				printf("%s:%d: ERROR: Invalid 'SET_VAR_TAGS(%s)'\n",__FUNCTION__,__LINE__, var.c_str());
				return false;
			}

			// Try to extract a tag for each iteration
			for (int iter = 1; iter < iteration; iter++)
			{
				if (!getValueParameter(lineStream, tag))
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
std::vector<testInformation> ScriptedSmokeTest::getTestInformation()
{
	std::vector<testInformation> testList;

	if (!getScriptDirectory())
	{
		printf("%s:%d: ERROR - unabe to find script directory\n",__FUNCTION__,__LINE__);
		return testList;
	}

	// Parse the test URL file
	std::string testUrlsFile = smoketestDirectory + "/" + SCRIPT_TEST_URL_FILE;
	getTestUrls(testUrlsFile);

	// Check the script directory - if the app has been run requesting the test of a specific script then
	// scriptDirectory will indicate the file to parse. In this case we will extract the path and filename
	// and limit the test cases to just this script.
	std::string specifiedScript;
	if (scriptDirectory.find(".smk") != std::string::npos)
	{
		std::size_t pos = scriptDirectory.rfind("/");
		if (pos != std::string::npos)
		{
			specifiedScript = scriptDirectory.substr(pos + 1);
			scriptDirectory = scriptDirectory.erase(pos);
		}
		else // No path so assume it is a script in the smoketest dir
		{
			specifiedScript = scriptDirectory;
			scriptDirectory = smoketestDirectory;
		}
	}

	DIR *directory = opendir(scriptDirectory.c_str());
	if (directory != NULL)
	{
		// Check all the files lloking for '.smk' scripts
		struct dirent *file = readdir(directory);
		while (file)
		{
			std::string filename(file->d_name);
			if (filename.size() > 4)
			{
				if (filename.substr(filename.size() - 4) == ".smk")
				{
					if (!specifiedScript.empty() && (filename != specifiedScript))
					{
						// Skip this as we have been given a speciific file to run and it doesn't match
					}
					else if (!checkTargetDevice(filename))
					{
						// Skip this test as it is not for this device
					}
					// Check for requested iterations and create tests
					else if (!applyTestIterations(filename, testList))
					{
						// Didn't find any iterations specified so just add the test
						testList.push_back(testInformation(filename));
					}
				}
			}

			file = readdir(directory);
		}

		closedir(directory);

		// Sort the tests alphabetically
		std::sort(testList.begin(), testList.end());
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
	std::stringstream text;
	std::string line;
	std::stringstream::pos_type loopstart;
	int loopcount = -1;
	bool scriptOk = true;
	int lineNumber = 0;
	bool abortOnError = false;

	printf("%s:%d: Run script %s...\n",__FUNCTION__,__LINE__, filename);
	std::ifstream script(getScriptFilePath(filename));
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
				//
				// Process commands that do not require a player / listener first
				//
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
				}
				else if (token == "new") // create a new player
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
					else if (!getIntParameter(cmd, loopcount))
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
				else if (token == "SET_STOP_ON_ERROR")
				{
					abortOnError = true;
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
#ifdef SIMULATOR_BUILD
				else if (token == "simlinear")
				{
				}
#endif
				else if (token.substr(0, 4) == "SET_")
				{
					// ignore
				}

				// All the following require a valid player/listener to be set
				else
				{
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

					//
					// Event listener commands
					//
					if (token == "waitfor") // check for specified events
					{
						std::string message;
						bool receivedEvents = listener->WaitForEvent(cmd, message);
						if (!receivedEvents)
						{
							printf("\n>>>>>>>>>>>>>>>>> Failure >>>>>>>>>>>>>>>>>>>>>>\n");
							if (gtest)
							{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
								EXPECT_TRUE(receivedEvents) << "line " << std::to_string(lineNumber) << ": " << message;
							}
							else
							{
								printf("%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
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
                    

						// If we delete the current player, try to find a valid one to continue with.
						// (Assuming here that the script will 'select' a valid player anyway.)
						while (currentPlayerId > 0)
						{
							if (mAampPlayer.getPlayer(currentPlayerId) != NULL)
							{
								break;
							}
							currentPlayerId--;
						}
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

					//
					// Check specified settings
					//
					else if (token == "check")
					{
						std::string message;
						bool checksOk = do_check(cmd, playerInstance, message);
						if (!checksOk)
						{
							printf("\n>>>>>>>>>>>>>>>>> Failure >>>>>>>>>>>>>>>>>>>>>>\n");
							if (gtest)
							{
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// GTEST bit
								EXPECT_TRUE(checksOk) << "line " << std::to_string(lineNumber) << ": " << message;
							}
							else
							{
								printf("%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
							}

							scriptOk = false;
						}
					}

					else
					{
						printf("%s:%d: ERROR: unrecognised command '%s'\n",__FUNCTION__,__LINE__, token.c_str());
						scriptOk = false;
					}
				}
			}

			//
			// If we have defined any events to fail the script on, check these here
			//
			if ((listener != NULL) && scriptOk)
			{
				std::string message;
				bool checkFailEvents = listener->CheckFailEvents(message);
				if (!checkFailEvents)
				{
					printf("\n>>>>>>>>>>>>>>>>> Failure >>>>>>>>>>>>>>>>>>>>>>\n");
					if (gtest)
					{
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GTEST bit
						EXPECT_TRUE(checkFailEvents) << message;
					}
					else
					{
						printf("%s:%d: Script failed: %s\n\n",__FUNCTION__,__LINE__, message.c_str());
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

	printf("\n%s:%d: >>> Script %s\n\n",__FUNCTION__,__LINE__, scriptOk?"SUCCEEDED":"FAILED");
	if (!scriptOk)
	{
		if (abortOnError)
		{
			printf("\n%s:%d: >>> Script has SET_STOP_ON_ERROR so skipping following tests\n\n",__FUNCTION__,__LINE__);
			setAbortTests();
		}

		//TBD - copy aamp log to an error log
		//std::string errlog = filename;
	}

	return scriptOk;
}

/**
 * @brief perform tune
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_tune(std::stringstream &args, PlayerInstanceAAMP *player)
{
	std::string url;
	uint32_t autoplay = 1;
	bool scriptOk = true;

	if (!getValueParameter(args, url))
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
	std::string var = url;
	if (getEnvironmentVariableName(var))
	{
		// First see if it is an exported variable
		if (getEnvironmentVariable(var, url))
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
 * @brief perform stop
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_stop(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Stop\n",__FUNCTION__,__LINE__);
	player->Stop();
	return true;
}

/**
 * @brief perform detach
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_detach(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Detach\n",__FUNCTION__,__LINE__);
	player->detach();
	return true;
}

/**
 * @brief perform setrate
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
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
 * @brief perform ff
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
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
 * @brief perform rew
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
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
 * @brief perform pause
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_pause(std::stringstream &args, PlayerInstanceAAMP *player)
{
	uint32_t position = 0;
	if (getUintParameter(args, position))
	{
		printf("%s:%d: PauseAt(%d)\n",__FUNCTION__,__LINE__, position);
		player->PauseAt(position);
	}
	else
	{
		printf("%s:%d: Pause\n",__FUNCTION__,__LINE__);
		player->SetRate(0);
	}
	return true;
}

/**
 * @brief perform play
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_play(std::stringstream &args, PlayerInstanceAAMP *player)
{
	printf("%s:%d: Play\n",__FUNCTION__,__LINE__);
	player->SetRate(1);
	return true;
}

/**
 * @brief perform seek
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_seek(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t position = 0;
	uint32_t keepPaused = 0;
	std::string name;
	std::string value;

	if (!getValueParameter(args, name, value, true, false)) // could be 'name(value)'
	{
		printf("%s:%d: Failed to get position param\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else if (getEnvironmentVariableName(name))
	{
		// First see if 'name' is an exported variable
		std::string exportedValue;
		if (getEnvironmentVariable(name, exportedValue))
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
				long durationSeconds = (listener->GetDuration() / 1000);
				position = (uint32_t)durationSeconds;
			}
			else
			{
				printf("%s:%d: ERROR accessing event listener\n",__FUNCTION__,__LINE__);
				scriptOk = false;
			}
		}
		else
		{
			printf("%s:%d: Unrecognised environment variable '%s'\n",__FUNCTION__,__LINE__, name.c_str());
			scriptOk = false;
		}

		if (scriptOk && !value.empty())
		{
			int adjust = 0;
			scriptOk = getInteger(value, adjust);
			position += adjust;
		}
	}
	else if (!getInteger(name, position))
	{
		printf("%s:%d: Failed to get position from '%s'\n",__FUNCTION__,__LINE__, name.c_str());
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
 * @brief perform async enable/disable
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
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
 * @brief perform config setting
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_config(std::stringstream &args, PlayerInstanceAAMP *player)
{
	std::string config = "{";
	std::string name;
	std::string value;
	int parameterCount = 0;
	while (getValueParameter(args, name, value, true, true, false)) // must have a name and value but the value can be empty
	{
		if (parameterCount)
		{
			config += ',';
		}

		config += "\"" + name + "\"" + ":";

		if (getIsNumber(value) || 
			(value == "true") || 
			(value == "false"))
		{
			config += value;
		}
		else
		{
			config += "\"" + value + "\"";
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
 * @brief perform setaudiotrack
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::process_setaudiotrack(std::stringstream &args, PlayerInstanceAAMP *player)
{
	bool scriptOk = true;
	uint32_t track = 0;

	std::string param;
	std::string value;
	if (!getValueParameter(args, param, value, true, false)) // get the argumnet
	{
		printf("%s:%d: Failed to get audio track\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else if (getInteger(param, track)) // if the argument is an integer call SetAudioTrack(track)
	{
		printf("%s:%d: Set audio track %d...\n",__FUNCTION__,__LINE__, track);
		player->SetAudioTrack(track);
	}
	else // assume a set of arguments of the form 'name(value)' eg 'language(eng,spa)'
	{
		std::string language="";
		std::string rendition="";
		std::string type="";
		std::string codec="";
		unsigned int channel=0;
		std::string label="";
		do
		{
			if (param == "language")
			{
				language = value;
			}
			else if (param == "rendition")
			{
				rendition = value;
			}
			else if (param == "type")
			{
				type = value;
			}
			else if (param == "codec")
			{
				codec = value;
			}
			else if (param == "channel")
			{
				if (!getInteger(param, channel))
				{
					printf("%s:%d: Unknown channel for SetAudioTrack(), '%s'\n",__FUNCTION__,__LINE__, param.c_str());
					scriptOk = false;
					break;
				}
			}
			else if (param == "label")
			{
				label = value;
			}
			else
			{
				printf("%s:%d: Unknown argumnet for SetAudioTrack(), '%s'\n",__FUNCTION__,__LINE__, param.c_str());
				scriptOk = false;
				break;
			}
		} while (getValueParameter(args, param, value));

		if (scriptOk)
		{
			printf("%s:%d: Set audio track (%s, %s, %s, %s, %d, %s)...\n",__FUNCTION__,__LINE__, 
			       language.c_str(), rendition.c_str(), type.c_str(), codec.c_str(), channel, label.c_str());
			player->SetAudioTrack(language, rendition, type, codec, channel, label);
		}
	}
	return scriptOk;
}

/**
 * @brief perform settexttrack
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
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



/**
 * @brief query and chck specified information
 * @param[in] args - a stream of command arguments
 * @param[in] player - the current player instance
 * @retval true if the command was parsed and sent
 */
bool ScriptedSmokeTest::do_check(std::stringstream &args, PlayerInstanceAAMP *player, std::string &status)
{
	bool scriptOk = true;
	std::string type;
	std::string settings;
	std::string jsonInfo;
	cJSON *jsonRoot = NULL;
	cJSON *jsonData = NULL;

	if (!getValueParameter(args, type, settings))
	{
		printf("%s:%d: ERROR - could not extract 'name(value)' to check\n",__FUNCTION__,__LINE__);
		scriptOk = false;
	}
	else if (type == "audio")
	{
		jsonInfo = player->GetAudioTrackInfo();
		jsonRoot = cJSON_Parse(jsonInfo.c_str());
		if (jsonRoot)
		{
			jsonData = jsonRoot->child;
			if (jsonData)
			{
				// Add the track index to the settings
				cJSON_AddItemToObject(jsonData, "track", cJSON_CreateNumber(player->GetAudioTrack()));
			}
		}
	}
	else if (type == "text")
	{
		jsonInfo = player->GetTextTrackInfo();
		jsonRoot = cJSON_Parse(jsonInfo.c_str());
		if (jsonRoot)
		{
			jsonData = jsonRoot->child;
			if (jsonData)
			{
				// Add the track index to the settings
				cJSON_AddItemToObject(jsonData, "track", cJSON_CreateNumber(player->GetTextTrack()));
			}
		}
	}
	else
	{
		printf("%s:%d: ERROR - unsupported check '%s'\n",__FUNCTION__,__LINE__, type.c_str());
		return false;
	}

	if (jsonData)
	{
		std::string name;
		std::string value;
		std::string actual;
		double dValue;

		std::stringstream settingsStream(settings);
		while (getValueParameter(settingsStream, name, value) && scriptOk)
		{
			cJSON *jsonSetting = NULL;
			for ( jsonSetting = jsonData->child; ((jsonSetting != NULL) && scriptOk); jsonSetting = jsonSetting->next)
			{
				if (jsonSetting->string)
				{
					if (name == jsonSetting->string)
					{
						if (jsonSetting->type == cJSON_String)
						{
							scriptOk = (value == jsonSetting->valuestring);
							actual = jsonSetting->valuestring;
						}
						else if (jsonSetting->type == cJSON_Number)
						{
							scriptOk = (getInteger(value, dValue) && (dValue == jsonSetting->valuedouble));
							actual = std::to_string(jsonSetting->valuedouble);
						}
						else if (jsonSetting->type == cJSON_False)
						{
							scriptOk = (value == "false");
							actual = "false";
						}
						else if (jsonSetting->type == cJSON_True)
						{
							scriptOk = (value == "true");
							actual = "true";
						}
						else
						{
							printf("%s:%d: ERROR - unsupported json type (%d)\n",__FUNCTION__,__LINE__, jsonSetting->type);
							actual = "<unsupported>";
							scriptOk = false;
						}

						if (!scriptOk)
						{
							status = "Check failed for " + type + " " + name + ", expected '" + value + "', got '" + actual + "'";
						}
						break;
					}
				}
			}

			if (jsonSetting == NULL)
			{
				scriptOk = false;
			}
		}

		cJSON_Delete(jsonRoot);
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
		if (ScriptedSmokeTest::getAbortTests())
		{
			GTEST_SKIP() << "SKIPPING TEST because script failed with SET_STOP_ON_ERROR";
		}
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
	EXPECT_TRUE(runScript(_scriptInfo.file.c_str(), _scriptInfo.iteration)) << _scriptInfo.file << _scriptInfo.name;;
}

// Create a test for each element in the vector returned by ScriptedSmokeTest::getTestInformation()
INSTANTIATE_TEST_SUITE_P(SmokeTestScripts, ScriptTest, 
                         ::testing::ValuesIn(ScriptedSmokeTest::getTestInformation()),
						 ::testing::PrintToStringParamName());

