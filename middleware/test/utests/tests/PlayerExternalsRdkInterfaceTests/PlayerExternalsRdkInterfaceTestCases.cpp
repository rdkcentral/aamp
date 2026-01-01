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
#include "PlayerExternalsRdkInterface.h"

#ifdef USE_DS_EVENT_SUPPORTED
#include "FakeDeviceSettings.h"
#endif

class PlayerExternalsRdkInterfaceTests : public ::testing::Test {
protected:
    std::shared_ptr<PlayerExternalsRdkInterface> mInterface;
    
    void SetUp() override {
#ifdef USE_DS_EVENT_SUPPORTED
        device::DeviceSettingsTestHelper::getInstance().reset();
        device::Manager::DeInitialize();
#endif
        mInterface = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
    }
    
    void TearDown() override {
        mInterface.reset();
#ifdef USE_DS_EVENT_SUPPORTED
        device::DeviceSettingsTestHelper::getInstance().reset();
        device::Manager::DeInitialize();
#endif
    }
};

// ========================================
// Initialization Tests
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, Constructor_CreatesInstance) {
    ASSERT_NE(nullptr, mInterface);
}

#ifdef USE_DS_EVENT_SUPPORTED

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_InitializesDeviceManager) {
    ASSERT_FALSE(device::Manager::isInitialized());
    
    mInterface->Initialize();
    
    EXPECT_TRUE(device::Manager::isInitialized());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersVideoOutputPortEvents) {
    mInterface->Initialize();
    
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersDisplayDeviceEvents) {
    mInterface->Initialize();
    
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersBothEventHandlers) {
    mInterface->Initialize();
    
    device::Host& host = device::Host::getInstance();
    EXPECT_GT(host.getRegisteredVideoHandlerCount(), 0);
    EXPECT_GT(host.getRegisteredDisplayHandlerCount(), 0);
}

TEST_F(PlayerExternalsRdkInterfaceTests, Destructor_UnregistersVideoHandlers) {
    mInterface->Initialize();
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
    
    mInterface.reset();
    
    EXPECT_FALSE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Destructor_UnregistersDisplayHandlers) {
    mInterface->Initialize();
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
    
    mInterface.reset();
    
    EXPECT_FALSE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Destructor_DeinitializesDeviceManager) {
    mInterface->Initialize();
    EXPECT_TRUE(device::Manager::isInitialized());
    
    mInterface.reset();
    
    EXPECT_FALSE(device::Manager::isInitialized());
}

// ========================================
// Event Handler Tests - HDMI Hotplug
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_Connected_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(
            dsDISPLAY_EVENT_CONNECTED
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_Disconnected_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(
            dsDISPLAY_EVENT_DISCONNECTED
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_MultipleEvents_HandledSequentially) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW({
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(dsDISPLAY_EVENT_CONNECTED);
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(dsDISPLAY_EVENT_DISCONNECTED);
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(dsDISPLAY_EVENT_CONNECTED);
    });
}

// ========================================
// Event Handler Tests - HDCP Status
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_Authenticated_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(
            dsHDCP_STATUS_AUTHENTICATED
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_Unauthenticated_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(
            dsHDCP_STATUS_UNAUTHENTICATED
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_AuthenticationFailure_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(
            dsHDCP_STATUS_AUTHENTICATIONFAILURE
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_Unpowered_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(
            dsHDCP_STATUS_UNPOWERED
        )
    );
}

// ========================================
// Event Handler Tests - Resolution Change
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPreChange_720p_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPreChange(1280, 720)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPreChange_1080p_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPreChange(1920, 1080)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_720p_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(1280, 720)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_1080p_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(1920, 1080)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_4K_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(3840, 2160)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_8K_DoesNotCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(7680, 4320)
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, ResolutionChangeSequence_PreThenPost_HandledCorrectly) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW({
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPreChange(1920, 1080);
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(1920, 1080);
    });
}

// ========================================
// Combined Event Tests
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, MultipleEventTypes_ProcessedCorrectly) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW({
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(dsDISPLAY_EVENT_CONNECTED);
        device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(dsHDCP_STATUS_AUTHENTICATED);
        device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(1920, 1080);
    });
}

TEST_F(PlayerExternalsRdkInterfaceTests, RapidEventSequence_HandledWithoutCrash) {
    mInterface->Initialize();
    
    ASSERT_NO_THROW({
        for (int i = 0; i < 10; i++) {
            device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(dsDISPLAY_EVENT_CONNECTED);
            device::DeviceSettingsTestHelper::getInstance().triggerHDCPStatusChange(dsHDCP_STATUS_AUTHENTICATED);
            device::DeviceSettingsTestHelper::getInstance().triggerResolutionPostChange(1920, 1080);
        }
    });
}

// ========================================
// Template Method Tests
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, BaseInterface_IVideoOutputPortEvents_ReturnsValidPointer) {
    auto* ptr = mInterface->baseInterface<device::Host::IVideoOutputPortEvents>();
    ASSERT_NE(nullptr, ptr);
}

TEST_F(PlayerExternalsRdkInterfaceTests, BaseInterface_IDisplayDeviceEvents_ReturnsValidPointer) {
    auto* ptr = mInterface->baseInterface<device::Host::IDisplayDeviceEvents>();
    ASSERT_NE(nullptr, ptr);
}

// ========================================
// Error Handling Tests
// ========================================

TEST_F(PlayerExternalsRdkInterfaceTests, EventsWithoutInitialize_DoNotCrash) {
    // Trigger events without calling Initialize()
    ASSERT_NO_THROW(
        device::DeviceSettingsTestHelper::getInstance().triggerHDMIHotPlug(
            dsDISPLAY_EVENT_CONNECTED
        )
    );
}

TEST_F(PlayerExternalsRdkInterfaceTests, DoubleInitialize_HandledGracefully) {
    ASSERT_NO_THROW({
        mInterface->Initialize();
        mInterface->Initialize(); // Initialize twice
    });
}

#else // !USE_DS_EVENT_SUPPORTED

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_WithoutDsEventSupport_DoesNotCrash) {
    ASSERT_NO_THROW(mInterface->Initialize());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Destructor_WithoutDsEventSupport_DoesNotCrash) {
    mInterface->Initialize();
    ASSERT_NO_THROW({
        mInterface.reset();
    });
}

#endif // USE_DS_EVENT_SUPPORTED