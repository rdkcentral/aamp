#ifndef DEVICE_IARM_INTERFACE_H
#define DEVICE_IARM_INTERFACE_H

#include "DeviceInterfaceBase.h"

#include <string>

class DeviceIARMInterface : public DeviceInterfaceBase {

    
    public:

    DeviceIARMInterface(const DeviceIARMInterface&) = delete;
    
    DeviceIARMInterface& operator=(const DeviceIARMInterface&) = delete;

    void RegisterDsMgrEventHandler() override;

    void RemoveDsMgrEventHandler() override;

    bool IsActiveStreamingInterfaceWifi() override;

    char *GetTR181Config(const char * paramName, size_t & iConfigLen) override;

    private:

    DeviceIARMInterface(std::string processName);

    static void IARMInit(const char* processName);

};

#endif