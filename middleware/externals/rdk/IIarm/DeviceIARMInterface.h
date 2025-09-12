#ifndef DEVICE_IARM_INTERFACE_H
#define DEVICE_IARM_INTERFACE_H

#include "DeviceInterfaceBase.h"

#include <string>

class DeviceIARMInterface : public DeviceInterfaceBase {

    
    public:

        DeviceIARMInterface(const DeviceIARMInterface&) = delete;
        
        DeviceIARMInterface& operator=(const DeviceIARMInterface&) = delete;

        char *GetTR181Config(const char * paramName, size_t & iConfigLen) override;

        static std::shared_ptr<DeviceIARMInterface> GetInstance();

        static void Initialize();

        ~DeviceIARMInterface();

    private:

        void RegisterDsMgrEventHandler() override;

        void RegisterNtwMgrEventHandler() override;

        void RemoveEventHandlers() override;

        DeviceIARMInterface();

        static void IARMInit();

};

#endif