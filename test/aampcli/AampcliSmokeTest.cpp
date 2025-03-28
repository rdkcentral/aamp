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

#include <time.h>
#include "Aampcli.h"
#include "AampcliSmokeTest.h"

extern Aampcli mAampcli;
static PlayerInstanceAAMP *mPlayerInstanceAamp;
std::map<std::string, std::string> SmokeTest::smokeTestUrls = std::map<std::string, std::string>();

bool SmokeTest::execute(const char *cmd, PlayerInstanceAAMP *playerInstanceAamp)
{
	mPlayerInstanceAamp = playerInstanceAamp;

	loadSmokeTestUrls();	
	vodTune("Dash");
	vodTune("Hls");
	liveTune("Live");

	return true;
}

void SmokeTest::loadSmokeTestUrls()
{

	const std::string smokeurlFile("/opt/smoketest.csv");
	int BUFFER_SIZE = 500;
	char buffer[BUFFER_SIZE];
	FILE *fp;

	if ((fp = mAampcli.getConfigFile(smokeurlFile)) != NULL)
	{
		printf("opened smoketest file\n");

		while (!feof(fp))
		{
			if (fgets(buffer, BUFFER_SIZE, fp) != NULL)
			{
				buffer[strcspn(buffer, "\n")] = 0;
				std::string urlData(buffer);

				std::size_t delimiterPos = urlData.find(",");

				if (delimiterPos != std::string::npos)
				{
					smokeTestUrls[urlData.substr(0, delimiterPos)] = urlData.substr(delimiterPos + 1);
				}
			}
		}

		fclose(fp);
	}
}

void SmokeTest::vodTune(const char *stream)
{
	const char *url = NULL;
	std::string fileName;
	std::string testFilePath;
	FILE *fp;
	time_t initialTime = 0, currentTime = 0;
	std::map<std::string, std::string>::iterator itr;

	createTestFilePath(testFilePath);
	if(strncmp(stream,"Dash",4) == 0)
	{
		fileName = testFilePath + "tuneDashStream.txt";
		itr = smokeTestUrls.find("VOD_DASH");

		itr = smokeTestUrls.find("VOD_DASH");

		if(itr != smokeTestUrls.end())
		{	
			url = (itr->second).c_str();
		}

		if(url == NULL)
		{
			url = "https://example.com/VideoTestStream/main.mpd";
		}
		
	}
	else if(strncmp(stream,"Hls",3) == 0)
	{
		fileName = testFilePath + "tuneHlsStream.txt";
		itr = smokeTestUrls.find("VOD_HLS");

		if(itr != smokeTestUrls.end())
		{	
			url = (itr->second).c_str();
		}

		if(url == NULL)
		{
			url = "https://example.com/VideoTestStream/main.m3u8";
		}
	}

	if(NULL == url)
	{
		printf("URL is InValid\n");
		return;
	}
	
	fp = stdout;
	stdout = fopen(fileName.c_str(),"w");

	mPlayerInstanceAamp->Tune(url);

	initialTime = time(NULL);

	while(1)
	{
		sleep(5);
		if(mPlayerInstanceAamp->GetState() == eSTATE_COMPLETE)
		{
			printf("Tune sub task started\n");
			mPlayerInstanceAamp->Tune(url);
			mPlayerInstanceAamp->SetRate(0); // To pause 
			mPlayerInstanceAamp->SetRate(4); // To fastforward 
			mPlayerInstanceAamp->SetRate(1); // To play
			sleep(20);
			mPlayerInstanceAamp->SetRate(-2); // To rewind
			sleep(10);
			mPlayerInstanceAamp->Stop();
			sleep(3);

			printf("Tune %s completed\n",stream);
			break;
		}

		currentTime = time(NULL);
		if((currentTime - initialTime) > 1200)
		{
			break;
		}
	}

	fclose(stdout);
	stdout = fp;
}

void SmokeTest::liveTune(const char *stream)
{
	const char *url = NULL;
	std::string fileName;
	std::string testFilePath;
	FILE *fp;

	createTestFilePath(testFilePath);

	url = "https://example.com/manifest.mpd";
	fileName = testFilePath +"tuneLive.txt";

	fp = stdout;
	stdout = fopen(fileName.c_str(),"w");

	mPlayerInstanceAamp->Tune(url);
	sleep(10);
	mPlayerInstanceAamp->SetRate(0); // To pause 
	mPlayerInstanceAamp->SetRate(1); // To play
	sleep(10);
	mPlayerInstanceAamp->Stop();
	sleep(5);

	fclose(stdout);
	stdout = fp;

}

bool SmokeTest::createTestFilePath(std::string &filePath)
{
	filePath = aamp_GetConfigPath("/opt/smoketest");
	DIR *dir = opendir(filePath.c_str());
	if (!dir)
	{
		if(mkdir(filePath.c_str(), 0777) == -1)
		{
			printf("Error:filePath=%s strerror=%s\n",filePath.c_str(),strerror(errno));
			return false;
		}
	}
	else
	{
		closedir(dir);
	}

	return true;
}

