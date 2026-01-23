#pragma once

#include "dsDisplay.h"
#include "dsMgr.h"
#include "videoOutputPort.hpp"
#include <string>

namespace device {
class Host {
public:
    struct IDisplayDeviceEvents {
        virtual ~IDisplayDeviceEvents() = default;
        virtual void OnDisplayHDMIHotPlug(dsDisplayEvent_t) = 0;
    };

    struct IVideoOutputPortEvents {
        virtual ~IVideoOutputPortEvents() = default;
        virtual void OnResolutionPreChange(const int, const int) = 0;
        virtual void OnResolutionPostChange(const int, const int) = 0;
        virtual void OnHDCPStatusChange(dsHdcpStatus_t) = 0;
    };

    static Host& getInstance() {
        static Host instance;
        return instance;
    }

    std::string getDefaultVideoPortName() const { return "HDMI0"; }
    
    VideoOutputPort& getVideoOutputPort(const std::string&) {
        static VideoOutputPort port;
        return port;
    }

    void Register(IDisplayDeviceEvents*, const char*) {}
    void Register(IVideoOutputPortEvents*, const char*) {}
    void UnRegister(IDisplayDeviceEvents*) {}
    void UnRegister(IVideoOutputPortEvents*) {}
};
}
