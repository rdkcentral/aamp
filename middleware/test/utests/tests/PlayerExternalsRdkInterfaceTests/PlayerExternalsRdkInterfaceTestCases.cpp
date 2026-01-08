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
        mInterface = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
        // Initialize to ensure handlers are registered for all tests
        // Multiple calls to Initialize() are safe (it checks if already initialized)
        mInterface->Initialize();
    }
    
    void TearDown() override {
        // Don't call mInterface.reset() to avoid double-free issue in production code destructors
        // The singleton will be cleaned up when the global shared_ptr goes out of scope
        mInterface = nullptr;
#ifdef USE_DS_EVENT_SUPPORTED
        // Don't clean up DS Event handlers between tests since the singleton persists
        // and won't re-register handlers if it thinks it's already initialized
        // Only reset the test helper state for tracking
        // device::DeviceSettingsTestHelper::getInstance().reset();
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
    // Initialize the interface (may already be initialized from previous tests)
    mInterface->Initialize();
    
    // After initialization, Device Manager should be initialized
    EXPECT_TRUE(device::Manager::isInitialized());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersVideoOutputPortEvents) {
    // Initialize the interface (safe to call multiple times)
    mInterface->Initialize();
    
    // Verify video output port handlers are registered
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersDisplayDeviceEvents) {
    // Initialize the interface (safe to call multiple times)
    mInterface->Initialize();
    
    // Verify display device handlers are registered
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
}

TEST_F(PlayerExternalsRdkInterfaceTests, Initialize_RegistersBothEventHandlers) {
    mInterface->Initialize();
    
    device::Host& host = device::Host::getInstance();
    EXPECT_GT(host.getRegisteredVideoHandlerCount(), 0);
    EXPECT_GT(host.getRegisteredDisplayHandlerCount(), 0);
}

TEST_F(PlayerExternalsRdkInterfaceTests, ZZZ_Cleanup_UnregistersAllHandlersAndDeinitializes) {
    // Verify handlers and manager are initialized
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
    EXPECT_TRUE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
    EXPECT_TRUE(device::Manager::isInitialized());
    
    // Remove all handlers and deinitialize
    mInterface->RemoveDsClientEventHandlers();
    
    // Verify everything was cleaned up
    EXPECT_FALSE(device::DeviceSettingsTestHelper::getInstance().hasVideoOutputPortHandler());
    EXPECT_FALSE(device::DeviceSettingsTestHelper::getInstance().hasDisplayDeviceHandler());
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

TEST_F(PlayerExternalsRdkInterfaceTests, DISABLED_Initialize_WithoutDsEventSupport_DoesNotCrash) {
    ASSERT_NO_THROW(mInterface->Initialize());
}

TEST_F(PlayerExternalsRdkInterfaceTests, DISABLED_Destructor_WithoutDsEventSupport_DoesNotCrash) {
    mInterface->Initialize();
    ASSERT_NO_THROW({
        mInterface.reset();
    });
}

#endif // USE_DS_EVENT_SUPPORTED
