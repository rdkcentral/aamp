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

bool Manager::sInitialized = false;

void Manager::Initialize() {
    sInitialized = true;
}

void Manager::DeInitialize() {
    sInitialized = false;
}

bool Manager::isInitialized() {
    return sInitialized;
}

Host& Host::getInstance() {
    static Host instance;
    return instance;
}

void Host::Register(Host::IVideoOutputPortEvents* handler, const std::string& name) {
    if (handler) {
        // Only increment count if we're replacing nullptr (no handler was registered before)
        if (!DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler()) {
            mVideoHandlerCount++;
        }
        DeviceSettingsTestHelper::getInstance().setVideoOutputPortHandler(handler);
    }
}

void Host::Register(Host::IDisplayDeviceEvents* handler, const std::string& name) {
    if (handler) {
        // Only increment count if we're replacing nullptr (no handler was registered before)
        if (!DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler()) {
            mDisplayHandlerCount++;
        }
        DeviceSettingsTestHelper::getInstance().setDisplayDeviceHandler(handler);
    }
}

void Host::UnRegister(Host::IVideoOutputPortEvents* handler) {
    DeviceSettingsTestHelper& helper = DeviceSettingsTestHelper::getInstance();
    // Only unregister if the handler matches the currently registered handler
    if (handler != nullptr && helper.getVideoOutputPortHandler() == handler && mVideoHandlerCount > 0) {
        helper.setVideoOutputPortHandler(nullptr);
        mVideoHandlerCount--;
    }
}

void Host::UnRegister(Host::IDisplayDeviceEvents* handler) {
    DeviceSettingsTestHelper& helper = DeviceSettingsTestHelper::getInstance();
    // Only unregister if the handler matches the currently registered handler
    if (handler != nullptr && helper.getDisplayDeviceHandler() == handler && mDisplayHandlerCount > 0) {
        helper.setDisplayDeviceHandler(nullptr);
        mDisplayHandlerCount--;
    }
}

int Host::getRegisteredVideoHandlerCount() const {
    return mVideoHandlerCount;
}

int Host::getRegisteredDisplayHandlerCount() const {
    return mDisplayHandlerCount;
}

VideoOutputPort& Host::getVideoOutputPort(const std::string& name) {
    return mVideoPort;
}

// Test Helper Implementation
DeviceSettingsTestHelper& DeviceSettingsTestHelper::getInstance() {
    static DeviceSettingsTestHelper instance;
    return instance;
}

void DeviceSettingsTestHelper::setVideoOutputPortHandler(Host::IVideoOutputPortEvents* handler) {
    mVideoOutputHandler = handler;
}

void DeviceSettingsTestHelper::setDisplayDeviceHandler(Host::IDisplayDeviceEvents* handler) {
    mDisplayDeviceHandler = handler;
}

Host::IVideoOutputPortEvents* DeviceSettingsTestHelper::getVideoOutputPortHandler() const {
    return mVideoOutputHandler;
}

Host::IDisplayDeviceEvents* DeviceSettingsTestHelper::getDisplayDeviceHandler() const {
    return mDisplayDeviceHandler;
}

void DeviceSettingsTestHelper::triggerResolutionPreChange(int width, int height) {
    if (mVideoOutputHandler) {
        mVideoOutputHandler->OnResolutionPreChange(width, height);
    }
}

void DeviceSettingsTestHelper::triggerResolutionPostChange(int width, int height) {
    if (mVideoOutputHandler) {
        mVideoOutputHandler->OnResolutionPostChange(width, height);
    }
}

void DeviceSettingsTestHelper::triggerHDCPStatusChange(dsHdcpStatus_t status) {
    if (mVideoOutputHandler) {
        mVideoOutputHandler->OnHDCPStatusChange(status);
    }
}

void DeviceSettingsTestHelper::triggerHDMIHotPlug(dsDisplayEvent_t event) {
    if (mDisplayDeviceHandler) {
        mDisplayDeviceHandler->OnDisplayHDMIHotPlug(event);
    }
}

void DeviceSettingsTestHelper::reset() {
    mVideoOutputHandler = nullptr;
    mDisplayDeviceHandler = nullptr;
}

bool DeviceSettingsTestHelper::hasVideoOutputPortHandler() const {
    return mVideoOutputHandler != nullptr;
}

bool DeviceSettingsTestHelper::hasDisplayDeviceHandler() const {
    return mDisplayDeviceHandler != nullptr;
}

} // namespace device