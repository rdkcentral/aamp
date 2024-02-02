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

/// A large count limit if not specified or if duration specified.
#define DEFAULT_HARVEST_COUNT_LIMIT (9999999)

class Harvester : public Command
{

	public:
		// static so can be used to size arrays below
		static const int mHarvestCommandLength = 4096;
	
		static PlayerInstanceAAMP *mPlayerInstanceAamp;
	
		std::thread mHarvestThreadID;
		
		// static so can be passed to thread()
		static void Harvest(void * arg);
		
		bool execute( const char *cmd, PlayerInstanceAAMP *playerInstanceAamp) override;
		Harvester();
};


#endif // AAMPCLIHARVESTER_H
