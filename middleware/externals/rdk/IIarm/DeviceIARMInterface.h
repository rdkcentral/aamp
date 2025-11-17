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
 * @file DeviceIARMInterface.h
 * @brief IARM interface
 */

/*
IARM Deprecation Note:
IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
*/
/*
Remove the entire folder externals/rdk/IARM
*/


#ifndef DEVICE_IARM_INTERFACE_H
#define DEVICE_IARM_INTERFACE_H

#include "DeviceInterfaceBase.h"
#include "host.hpp"
#include <string>

class DeviceIARMInterface : public DeviceInterfaceBase,
	public device::Host::IDisplayDeviceEvents,
	public device::Host::IVideoOutputPortEvents {
    public:
	template <typename T>
	   T* baseInterface()
	{
	    static_assert(std::is_base_of<T, DeviceIARMInterface>(), "base type mismatch");
	    return static_cast<T*>(this);
	}

	DeviceIARMInterface(const DeviceIARMInterface&) = delete;
        
        DeviceIARMInterface& operator=(const DeviceIARMInterface&) = delete;

        char *GetTR181Config(const char * paramName, size_t & iConfigLen) override;

        static std::shared_ptr<DeviceIARMInterface> GetInstance();

        static void Initialize();

        /* IVideoOutputPortEvents */
        void OnResolutionPreChange(const int width, const int height) override;
        void OnResolutionPostChange(const int width, const int height) override;
        void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus);

        /* IDisplayDeviceEvents */
        void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent) override;

        ~DeviceIARMInterface();

    private:

        void RegisterDsMgrEventHandler() override;

        void RegisterNtwMgrEventHandler() override;

        void RemoveEventHandlers() override;

        DeviceIARMInterface();

        static void IARMInit();

};

#endif
