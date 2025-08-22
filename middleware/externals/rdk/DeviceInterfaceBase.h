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