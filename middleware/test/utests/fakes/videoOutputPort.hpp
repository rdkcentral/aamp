#pragma once

#include "videoResolution.hpp"
#include "dsMgr.h"
#include "dsDisplay.h"

namespace device {

class VideoOutputPort {
public:
    bool isDisplayConnected() const { return true; }
    int getHDCPProtocol() const { return dsHDCP_VERSION_2X; }
    dsHdcpStatus_t getHDCPStatus() const { return dsHDCP_STATUS_AUTHENTICATED; }
    bool isContentProtected() const { return true; }
    int getHDCPReceiverProtocol() const { return dsHDCP_VERSION_2X; }
    int getHDCPCurrentProtocol() const { return dsHDCP_VERSION_2X; }
    VideoResolution getResolution() const { return VideoResolution(); }
};

}
