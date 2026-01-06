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
 * @file DeviceInterfaceBase.h
 * @brief base class for device api interface
 */

 /*
IARM Deprecation Note:
IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
*/
/*
Remove the entire file externals/DeviceInterfaceBase.h when deprecating IARM
*/


#ifndef DEVICE_INTERFACE_BASE_H
#define DEVICE_INTERFACE_BASE_H

#include <cstddef>
#include <memory>

class DeviceInterfaceBase {

    public:

    DeviceInterfaceBase()
    {}

    virtual void RegisterDsMgrEventHandler() = 0;

    virtual void RegisterNtwMgrEventHandler() = 0;

    virtual void RemoveEventHandlers() = 0;

    virtual char *GetTR181Config(const char * paramName, size_t & iConfigLen) = 0;

};

#endif