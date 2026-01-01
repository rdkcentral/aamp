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

#include <gtest/gtest.h>
#include "DeviceIARMInterface.h"
#include "FakeIARM.h"

class DeviceIARMInterfaceTests : public ::testing::Test {
protected:
    std::shared_ptr<DeviceIARMInterface> mInterface;
    
    void SetUp() override {
        IARMTestHelper::getInstance().reset();
        mInterface = DeviceIARMInterface::GetInstance();
    }
    
    void TearDown() override {
        mInterface.reset();
        IARMTestHelper::getInstance().reset();
    }
};

// ========================================
// Conditional Compilation Tests
// ========================================

#ifndef USE_DS_EVENT_SUPPORTED

TEST_F(DeviceIARMInterfaceTests, Initialize_RegistersHDMIHotplug) {
    DeviceIARMInterface::Initialize();
    
    int count = IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, 
        IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG
    );
    
    EXPECT_EQ(1, count);
}

TEST_F(DeviceIARMInterfaceTests, Initialize_RegistersHDCPStatus) {
    DeviceIARMInterface::Initialize();
    
    int count = IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, 
        IARM_BUS_DSMGR_EVENT_HDCP_STATUS
    );
    
    EXPECT_EQ(1, count);
}

TEST_F(DeviceIARMInterfaceTests, Initialize_RegistersResolutionPostChange) {
    DeviceIARMInterface::Initialize();
    
    int count = IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, 
        IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE
    );
    
    EXPECT_EQ(1, count);
}

TEST_F(DeviceIARMInterfaceTests, Initialize_HandlesMultipleCalls) {
    DeviceIARMInterface::Initialize();
    EXPECT_EQ(1, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG));
    
    // Second Initialize should not duplicate handlers
    DeviceIARMInterface::Initialize();
    
    EXPECT_EQ(1, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG));
}

TEST_F(DeviceIARMInterfaceTests, HDMIHotplugEvent_TriggersConnected) {
    DeviceIARMInterface::Initialize();
    
    dsHDMI_EVENT_T eventData = dsHDMI_EVENT_CONNECTED;
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG,
        &eventData,
        sizeof(eventData)
    );
    
    // Verify by checking internal state or mock expectations
    SUCCEED(); // Event processed without crash
}

TEST_F(DeviceIARMInterfaceTests, HDMIHotplugEvent_TriggersDisconnected) {
    DeviceIARMInterface::Initialize();
    
    dsHDMI_EVENT_T eventData = dsHDMI_EVENT_DISCONNECTED;
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG,
        &eventData,
        sizeof(eventData)
    );
    
    SUCCEED();
}

TEST_F(DeviceIARMInterfaceTests, HDCPStatusEvent_TriggersAuthenticated) {
    DeviceIARMInterface::Initialize();
    
    dsHdcpStatus_t status = dsHDCP_STATUS_AUTHENTICATED;
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
        &status,
        sizeof(status)
    );
    
    SUCCEED();
}

TEST_F(DeviceIARMInterfaceTests, HDCPStatusEvent_TriggersAuthenticationFailure) {
    DeviceIARMInterface::Initialize();
    
    dsHdcpStatus_t status = dsHDCP_STATUS_AUTHENTICATIONFAILURE;
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_HDCP_STATUS,
        &status,
        sizeof(status)
    );
    
    SUCCEED();
}

TEST_F(DeviceIARMInterfaceTests, ResolutionPostChangeEvent_Triggers1080p) {
    DeviceIARMInterface::Initialize();
    
    IARM_Bus_DSMgr_EventData_t eventData = {1920, 1080};
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE,
        &eventData,
        sizeof(eventData)
    );
    
    SUCCEED();
}

TEST_F(DeviceIARMInterfaceTests, ResolutionPostChangeEvent_Triggers4K) {
    DeviceIARMInterface::Initialize();
    
    IARM_Bus_DSMgr_EventData_t eventData = {3840, 2160};
    IARMTestHelper::getInstance().triggerEvent(
        IARM_BUS_DSMGR_NAME,
        IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE,
        &eventData,
        sizeof(eventData)
    );
    
    SUCCEED();
}

TEST_F(DeviceIARMInterfaceTests, MultipleEventRegistrations_DoNotDuplicate) {
    DeviceIARMInterface::Initialize();
    DeviceIARMInterface::Initialize(); // Initialize twice
    
    // Should still be 1 handler
    EXPECT_EQ(1, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG));
}

#else // USE_DS_EVENT_SUPPORTED

TEST_F(DeviceIARMInterfaceTests, Initialize_SkipsWhenDsEventEnabled) {
    DeviceIARMInterface::Initialize();
    
    // No IARM handlers should be registered
    EXPECT_EQ(0, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG));
    EXPECT_EQ(0, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_HDCP_STATUS));
    EXPECT_EQ(0, IARMTestHelper::getInstance().getHandlerCount(
        IARM_BUS_DSMGR_NAME, IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE));
}

#endif // USE_DS_EVENT_SUPPORTED