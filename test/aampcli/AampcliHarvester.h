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
 * @file AampcliHarvester.h
 * @brief AampcliHarvester header file
 */

#ifndef AAMPCLIHARVESTER_H
#define AAMPCLIHARVESTER_H

#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#include <mutex>
#include "AampcliCommand.h"

#ifdef __APPLE__
    #include <mach-o/dyld.h>
#endif

#ifdef __linux__
    #if defined(__sun)
        #define PROC_SELF_EXE "/proc/self/path/a.out"
    #else
        #define PROC_SELF_EXE "/proc/self/exe"
    #endif
#endif

typedef struct harvestProfileDetails
{
	bool harvestEndFlag;
	char media[7];
	int harvestConfig;
	int harvestFragmentsCount;
	int harvestErrorCount;
	int harvestFailureCount;
	int harvestTrackId;
	long bitrate;
}HarvestProfileDetails;

class Harvester : public Command
{

	public:
		// static so can be used to size arrays below
		static const int mHarvestCommandLength = 4096;
		static const int mHarvestSlaveThreadCount = 50;
		static const int mHarvestSubsThreadCount = 10;
	
		// static so can be used in static function harvestTerminateHandler
		static bool mHarvestReportFlag;
		static std::string mHarvestPath;
		static std::mutex mHarvestInfoMutex;
		static std::map<std::thread::id, harvestProfileDetails> mHarvestInfo;
		static std::vector<std::thread::id> mHarvestThreadId;
	
		// static as used by slave harvester
		static PlayerInstanceAAMP *mPlayerInstanceAamp;
	
		char mExePathName[PATH_MAX];
		int mHarvestCountLimit;
		int mHarvestConfig;
		int mTCPServerSinkPort;

		std::thread mMasterHarvesterThreadID;
		std::thread mSlaveHarvesterThreadID;
		std::thread mReportThread;
		std::thread mSlaveIFrameThread;
		std::thread mSlaveVideoThreads[mHarvestSlaveThreadCount];
		std::thread mSlaveAudioThreads[mHarvestSlaveThreadCount];
		std::thread mSlaveSubtitleThreads[mHarvestSubsThreadCount];
		
		// static so can be passed to thread()
		static void masterHarvester(void * arg);
		static void slaveHarvester(void * arg);
		static void slaveDataOutput(void * arg);
		// static so can be passed to signal()
		static void harvestTerminateHandler(int signal);
	
		long getNumberFromString(std::string buffer);
		void startHarvestReport(char * arg);
		bool getHarvestReportDetails(char *buffer);
		FILE *createSlaveHarvester(std::map<std::string, std::string> cmdlineParams, int harvestConfig, long bitRate=0,
								   std::string language = "", int trackId=-1);
		bool createSlaveDataReader(FILE *pSlaveHarvester, std::thread& dataReader);
		void writeHarvestErrorReport(HarvestProfileDetails, char *buffer);
		void writeHarvestEndReport(HarvestProfileDetails, char *buffer);
		
		void getExecutablePath();
		bool execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp) override;
		Harvester();

		std::vector<AudioTrackInfo> GetAudioTracks();
		std::vector<TextTrackInfo> GetTextTracks();
};


#endif // AAMPCLIHARVESTER_H
