/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file DeviceFireboltInterface.h
 * @brief Firebolt device api interface
 */


/*
IARM Deprecation Note:
IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
*/

/*
Move the entire folder externals/rdk/IFirebolt 's contents to externals/rdk/
IFirebolt folder to be deleted, as IARM is no longer available as an alternative
*/


#ifndef DEVICE_FIREBOLT_INTERFACE_H
#define DEVICE_FIREBOLT_INTERFACE_H

#include "DeviceInterfaceBase.h"
#include "FireboltInterface.h"

#include <string>
#include <vector>

class DeviceFireboltInterface : public DeviceInterfaceBase {

    
    public:

        DeviceFireboltInterface(const DeviceFireboltInterface&) = delete;
        
        DeviceFireboltInterface& operator=(const DeviceFireboltInterface&) = delete;

        ~DeviceFireboltInterface();

        char *GetTR181Config(const char * paramName, size_t & iConfigLen) override;

        static std::shared_ptr<DeviceFireboltInterface> GetInstance();

        static void Initialize();

    private:

        std::shared_ptr<FireboltInterface> m_pFireboltInterface;

        std::vector<uint64_t> mDsMgrSubscriptionId;

        std::vector<uint64_t> mNtwMgrSubscriptionId;

        DeviceFireboltInterface();

        bool CreateFireboltInstance(const std::string &url);

        void ConnectionChanged(const bool connected, int error);

        void RegisterDsMgrEventHandler() override;

        void RegisterNtwMgrEventHandler() override;

        void RemoveEventHandlers() override;

        void DestroyFireboltInstance();

};

#endif