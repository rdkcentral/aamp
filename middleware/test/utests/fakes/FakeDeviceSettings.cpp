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

#include "FakeDeviceSettings.h"
#include <stdexcept>

namespace device {

bool Manager::initialized = false;

void Manager::Initialize() {
    initialized = true;
}

void Manager::DeInitialize() {
    initialized = false;
}

bool Manager::isInitialized() {
    return initialized;
}

Host& Host::getInstance() {
    static Host instance;
    return instance;
}

void Host::Register(Host::IVideoOutputPortEvents* handler, const std::string& name) {
    if (handler) {
        DeviceSettingsTestHelper::getInstance().setVideoOutputPortHandler(handler);
        videoHandlerCount++;
    }
}

void Host::Register(Host::IDisplayDeviceEvents* handler, const std::string& name) {
    if (handler) {
        DeviceSettingsTestHelper::getInstance().setDisplayDeviceHandler(handler);
        displayHandlerCount++;
    }
}

void Host::UnRegister(Host::IVideoOutputPortEvents* handler) {
    if (videoHandlerCount > 0) {
        DeviceSettingsTestHelper::getInstance().setVideoOutputPortHandler(nullptr);
        videoHandlerCount--;
    }
}

void Host::UnRegister(Host::IDisplayDeviceEvents* handler) {
    if (displayHandlerCount > 0) {
        DeviceSettingsTestHelper::getInstance().setDisplayDeviceHandler(nullptr);
        displayHandlerCount--;
    }
}

int Host::getRegisteredVideoHandlerCount() const {
    return videoHandlerCount;
}

int Host::getRegisteredDisplayHandlerCount() const {
    return displayHandlerCount;
}

VideoOutputPort& Host::getVideoOutputPort(const std::string& name) {
    return videoPort;
}

// Test Helper Implementation
DeviceSettingsTestHelper& DeviceSettingsTestHelper::getInstance() {
    static DeviceSettingsTestHelper instance;
    return instance;
}

void DeviceSettingsTestHelper::setVideoOutputPortHandler(Host::IVideoOutputPortEvents* handler) {
    videoOutputHandler = handler;
}

void DeviceSettingsTestHelper::setDisplayDeviceHandler(Host::IDisplayDeviceEvents* handler) {
    displayDeviceHandler = handler;
}

void DeviceSettingsTestHelper::triggerResolutionPreChange(int width, int height) {
    if (videoOutputHandler) {
        videoOutputHandler->OnResolutionPreChange(width, height);
    }
}

void DeviceSettingsTestHelper::triggerResolutionPostChange(int width, int height) {
    if (videoOutputHandler) {
        videoOutputHandler->OnResolutionPostChange(width, height);
    }
}

void DeviceSettingsTestHelper::triggerHDCPStatusChange(dsHdcpStatus_t status) {
    if (videoOutputHandler) {
        videoOutputHandler->OnHDCPStatusChange(status);
    }
}

void DeviceSettingsTestHelper::triggerHDMIHotPlug(dsDisplayEvent_t event) {
    if (displayDeviceHandler) {
        displayDeviceHandler->OnDisplayHDMIHotPlug(event);
    }
}

void DeviceSettingsTestHelper::reset() {
    videoOutputHandler = nullptr;
    displayDeviceHandler = nullptr;
}

bool DeviceSettingsTestHelper::hasVideoOutputPortHandler() const {
    return videoOutputHandler != nullptr;
}

bool DeviceSettingsTestHelper::hasDisplayDeviceHandler() const {
    return displayDeviceHandler != nullptr;
}

} // namespace device