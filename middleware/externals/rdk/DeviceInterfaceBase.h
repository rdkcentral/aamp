#ifndef DEVICE_INTERFACE_BASE_H
#define DEVICE_INTERFACE_BASE_H

#include <cstddef>
#include <memory>

class DeviceInterfaceBase {

    public:

    DeviceInterfaceBase()
    {}

    virtual void RegisterDsMgrEventHandler() = 0;

    virtual void RemoveEventHandler() = 0;

    //ToDo : rename below, it basically registers IARM_BUS_NETWORK_MANAGER_EVENT_INTERFACE_IPADDRESS and retuens truw if wifi
    virtual bool IsActiveStreamingInterfaceWifi() = 0;

    virtual char *GetTR181Config(const char * paramName, size_t & iConfigLen) = 0;

};

#endif