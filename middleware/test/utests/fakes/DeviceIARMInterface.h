#pragma once

#include <memory>
#include "DeviceInterfaceBase.h"

class DeviceIARMInterface : public DeviceInterfaceBase {
public:
    virtual ~DeviceIARMInterface() = default;
    
    static std::shared_ptr<DeviceInterfaceBase> GetInstance();
    static void Initialize();
    
    void RegisterDsMgrEventHandler() override;
    void RegisterNtwMgrEventHandler() override;
    void RemoveEventHandlers() override;
    char* GetTR181Config(const char* paramName, size_t& iConfigLen) override;
};
