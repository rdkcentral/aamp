/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
* @file ProcessHandler.h
* @brief Context-free process utility functions.
*/

#ifndef __PROCESS_HANDLER_H__
#define __PROCESS_HANDLER_H__

#include <string>
/**
 * @class ProcessHandler
 * @brief Class for controlling process from AAMP
 */
class ProcessHandler
{
    const std::string PROCESS_PROC_DIR = "/proc/";
    const std::string PROCESS_PROC_STATUS = "/status";
    const unsigned int BASE_NUMBER = 10;

    /**
     * @fn splitName
     */
    std::string splitName(std::string& line );

    /**
     * @fn convertPid
     */
    long convertPid(std::string& name);

    public:

    /**
     * @fn constructor - ProcessHandler
     */
    ProcessHandler()
    {
    }

    /**
     * @brief ProcessHandler Destructor function
     */
    ~ProcessHandler(){};

    /**
     * @brief Copy constructor
     */
    ProcessHandler(const ProcessHandler&) = delete;

    /**
     * @brief ProcessHandler assignment operator overloading
     */
    ProcessHandler& operator=(const ProcessHandler&) = delete;

    /**
     * @fn GetProcessName
     */
    std::string GetProcessName(std::string& pid);

    /**
     * @fn KillProcess
     */
    bool KillProcess(std::string processName);

    /**
     * @fn SelfKill
     */
    bool SelfKill(void);

    /**
     * @fn KillProcess
     */
    bool KillProcess(long pid);
};

#endif
/**
 * EOF
 */
