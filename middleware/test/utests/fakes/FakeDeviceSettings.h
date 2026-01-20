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

#ifndef FAKE_DEVICE_SETTINGS_H
#define FAKE_DEVICE_SETTINGS_H

#include <string>
#include <vector>
#include "FakeIARM.h"  // For dsHdcpStatus_t definition

// DeviceSettings type definitions
typedef enum {
    dsDISPLAY_EVENT_CONNECTED = 0,
    dsDISPLAY_EVENT_DISCONNECTED
} dsDisplayEvent_t;

// Note: dsHdcpStatus_t is defined in FakeIARM.h to avoid conflicts

namespace device {

// Forward declarations
class Manager;
class Host;
class VideoOutputPort;

// Pixel resolution enum
class PixelResolution {
public:
    static constexpr int k720x480 = 0;
    static constexpr int k720x576 = 1;
    static constexpr int k1280x720 = 2;
    static constexpr int k1920x1080 = 3;
    static constexpr int k3840x2160 = 4;
    static constexpr int k4096x2160 = 5;
    
    int getId() const { return mId; }
    std::string getName() const { return "1920x1080"; }
private:
    int mId = k1920x1080;
};

class Resolution {
public:
    PixelResolution getPixelResolution() const { return PixelResolution(); }
    std::string getName() const { return "1920x1080"; }
};

// VideoOutputPort stub class
class VideoOutputPort {
public:
    bool isDisplayConnected() const { return true; }
    Resolution getResolution() const { return Resolution(); }
    int getHDCPProtocol() const { return 0; }
    int getHDCPStatus() const { return dsHDCP_STATUS_AUTHENTICATED; }
    bool isContentProtected() const { return true; }
    int getHDCPReceiverProtocol() const { return 0; }
    int getHDCPCurrentProtocol() const { return 0; }
};

// Forward declarations
class Manager;
class Host;

class Manager {
public:
    static void Initialize();
    static void DeInitialize();
    static bool isInitialized();

private:
    static bool sInitialized;
};

class Host {
public:
    // Interface for Video Output Port Events - nested within Host
    class IVideoOutputPortEvents {
    public:
        virtual ~IVideoOutputPortEvents() = default;
        virtual void OnResolutionPreChange(const int width, const int height) = 0;
        virtual void OnResolutionPostChange(const int width, const int height) = 0;
        virtual void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus) = 0;
    };

    // Interface for Display Device Events - nested within Host
    class IDisplayDeviceEvents {
    public:
        virtual ~IDisplayDeviceEvents() = default;
        virtual void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent) = 0;
    };

    static Host& getInstance();
    
    void Register(IVideoOutputPortEvents* handler, const std::string& name);
    void Register(IDisplayDeviceEvents* handler, const std::string& name);
    void UnRegister(IVideoOutputPortEvents* handler);
    void UnRegister(IDisplayDeviceEvents* handler);
    
    int getRegisteredVideoHandlerCount() const;
    int getRegisteredDisplayHandlerCount() const;
    
    std::string getDefaultVideoPortName() const { return "HDMI0"; }
    VideoOutputPort& getVideoOutputPort(const std::string& name);

    friend class DeviceSettingsTestHelper;

private:
    int mVideoHandlerCount = 0;
    int mDisplayHandlerCount = 0;
    VideoOutputPort mVideoPort;
};

// Test helper to track and trigger events
class DeviceSettingsTestHelper {
public:
    static DeviceSettingsTestHelper& getInstance();
    void setVideoOutputPortHandler(Host::IVideoOutputPortEvents* handler);
    void setDisplayDeviceHandler(Host::IDisplayDeviceEvents* handler);
    void triggerResolutionPreChange(int width, int height);
    void triggerResolutionPostChange(int width, int height);
    void triggerHDCPStatusChange(dsHdcpStatus_t status);
    void triggerHDMIHotPlug(dsDisplayEvent_t event);
    void reset();
    bool hasVideoOutputPortHandler() const;
    bool hasDisplayDeviceHandler() const;
    // Expose current registered handlers for validation in UnRegister
    Host::IVideoOutputPortEvents* getVideoOutputPortHandler() const;
    Host::IDisplayDeviceEvents* getDisplayDeviceHandler() const;

private:
    Host::IVideoOutputPortEvents* mVideoOutputHandler = nullptr;
    Host::IDisplayDeviceEvents* mDisplayDeviceHandler = nullptr;
};

} // namespace device

#endif // FAKE_DEVICE_SETTINGS_H