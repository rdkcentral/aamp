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
 */

#ifndef DEVICE_FIREBOLT_INTERFACE_H
#define DEVICE_FIREBOLT_INTERFACE_H

#include "DeviceInterfaceBase.h"
#include <memory>

// Stub for DeviceFireboltInterface
class DeviceFireboltInterface : public DeviceInterfaceBase {
public:
    static std::shared_ptr<DeviceFireboltInterface> GetInstance() {
        return nullptr;
    }
    
    static void Initialize() {}
    
    void RegisterDsMgrEventHandler() override {}
    void RegisterNtwMgrEventHandler() override {}
    void RemoveEventHandlers() override {}
    char *GetTR181Config(const char * paramName, size_t & iConfigLen) override {
        return nullptr;
    }
};

#endif // DEVICE_FIREBOLT_INTERFACE_H
