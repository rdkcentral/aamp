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
 * @file ProcessHandler.cpp
 * @brief Process utility functions
 */


#include "ProcessHandler.h"
#include "PlayerUtils.h"
#include "PlayerLogManager.h"
#include <string>
#include <fstream>
#include <algorithm>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Parse the process info line and get the name of process
 *
 * @param proc status line
 * @retval -  process name
 */
std::string ProcessHandler::splitName(std::string& line )
{
        std::string delimiter = ":";
        std::string token = line.substr(line.find(delimiter) + delimiter.length());
        trim(token);
        return token;
}

/**
 * @brief convert the string pid value to long
 *
 * @param process pid as string
 * @retval - pid ; if failed -1
 */
long ProcessHandler::convertPid(std::string& name)
{
    char* p;
    long  pid = -1;
    pid = strtol(name.c_str(), &p, BASE_NUMBER);
    if( *p != 0)
       pid = -1;
    return pid;
}

/**
 * @brief Get the process name from Pid
 *
 * @param process pid as string
 * @retval - true or false
 */
std::string ProcessHandler::GetProcessName(std::string& pid)
{
    std::fstream newFile;
    std::string procPath = PROCESS_PROC_DIR+pid+PROCESS_PROC_STATUS;
    std::string line;
    std::string output = "";
    newFile.open(procPath, std::ios::in);
    if (newFile.is_open())
    {
        while(getline(newFile, line))
        {
            if (line.find("Name:") != std::string::npos)
            {
                output = splitName(line);
                break;
            }
        }
        newFile.close();
    }
    return output;
}


/**
 * @brief kill the process by name
 *
 * @param process name to kill
 * @retval - true or false
 */
bool ProcessHandler::KillProcess(std::string processName)
{
    bool status = false;
    struct dirent *entry;
    DIR *dir = opendir(PROCESS_PROC_DIR.c_str());
    if (dir == NULL) {
        return status;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        std::string strPid = std::string(entry->d_name);
        long lPid = convertPid(strPid);
        if (lPid > 0)
        {
            std::string name = GetProcessName(strPid);
            if (name == processName)
            {
                MW_LOG_INFO("Killing the process %s PID %ld", name.c_str(), lPid); 
                status = KillProcess(lPid);
                break; /**< No need to move further*/
            }
        }
    }
    closedir(dir);
    return status;
}

/**
 * @brief self kill the process
 *
 * @retval - true or false
 */
bool ProcessHandler::SelfKill()
{
    return KillProcess(getpid());
}

/**
 * @brief kill the process by pid number
 *
 * @param pid
 * @retval - true or false
 */
bool ProcessHandler::KillProcess(long pid)
{
    bool ret = true;
    if(kill(pid, SIGKILL) < 0)
    {
        MW_LOG_WARN("Kill Failed = %d", errno);
        ret = false;
    }
    return ret;
}

/**
 * EOF
 */
