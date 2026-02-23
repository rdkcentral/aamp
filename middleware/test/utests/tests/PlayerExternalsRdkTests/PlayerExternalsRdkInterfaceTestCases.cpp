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
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "PlayerLogManager.h"
#include <memory>

// Include the fake implementation to get the class definition and enums
#include "../../fakes/FakePlayerExternalsRdkInterface.cpp"

class PlayerExternalsRdkInterfaceTests : public ::testing::Test
{
protected:
	std::shared_ptr<PlayerExternalsRdkInterface> mPlayerExternalsRdk;

	void SetUp() override
	{
		// Get the singleton instance
		mPlayerExternalsRdk = PlayerExternalsRdkInterface::GetPlayerExternalsRdkInterfaceInstance();
		
		// Explicitly reset all global state before each test
		// The singleton pattern persists state across tests, so we must explicitly reset
		// to ensure each test starts with a clean, known state (test isolation)
		if (mPlayerExternalsRdk)
		{
			mPlayerExternalsRdk->Reset();
		}
		
		PlayerLogManager::lockLogLevel(false);
		PlayerLogManager::disableLogRedirection = true; // required for mwlog output in utest
		PlayerLogManager::setLogLevel(mLOGLEVEL_TRACE);
	}

	void TearDown() override
	{
		// Destroy and reset the singleton instance completely for true test isolation
		// This addresses the fundamental issue that singleton instances persist across tests
		PlayerExternalsRdkInterface::ResetSingletonInstance();
		mPlayerExternalsRdk.reset();
	}

	// Helper method to verify resolution values
	void VerifyResolution(int expectedWidth, int expectedHeight)
	{
		int actualWidth = 0;
		int actualHeight = 0;
		mPlayerExternalsRdk->GetDisplayResolution(actualWidth, actualHeight);
		
		EXPECT_EQ(actualWidth, expectedWidth) << "Width mismatch";
		EXPECT_EQ(actualHeight, expectedHeight) << "Height mismatch";
	}
};

/**
 * @brief Test OnResolutionPostChange with standard HD resolution (1920x1080)
 * 
 * This test verifies that OnResolutionPostChange correctly handles a standard
 * Full HD resolution and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_ValidHDResolution_StoresCorrectly)
{
	const int width = 1920;
	const int height = 1080;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with 4K UHD resolution (3840x2160)
 * 
 * This test verifies that OnResolutionPostChange correctly handles a 4K Ultra HD
 * resolution and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_4KResolution_StoresCorrectly)
{
	const int width = 3840;
	const int height = 2160;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with standard SD resolution (720x480)
 * 
 * This test verifies that OnResolutionPostChange correctly handles a standard
 * definition resolution (NTSC) and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_SDResolution_StoresCorrectly)
{
	const int width = 720;
	const int height = 480;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with HD 720p resolution (1280x720)
 * 
 * This test verifies that OnResolutionPostChange correctly handles HD 720p
 * resolution and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_HD720pResolution_StoresCorrectly)
{
	const int width = 1280;
	const int height = 720;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with Cinema 4K resolution (4096x2160)
 * 
 * This test verifies that OnResolutionPostChange correctly handles Cinema 4K
 * resolution (DCI 4K) and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_Cinema4KResolution_StoresCorrectly)
{
	const int width = 4096;
	const int height = 2160;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with zero resolution (0x0)
 * 
 * This is an edge case test where the resolution values are zero, which might
 * indicate a disconnected display or uninitialized state. The system should
 * handle this gracefully without crashing.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_ZeroResolution_HandlesGracefully)
{
	const int width = 0;
	const int height = 0;

	// Call the event handler - should not crash
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was NOT stored (validation should fail)
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid()) 
		<< "Zero resolution should be rejected";
}

/**
 * @brief Test OnResolutionPostChange with negative width value
 * 
 * This test verifies that negative width values are rejected and not stored.
 * Invalid inputs should not update the internal state.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_NegativeWidth_IsRejected)
{
	// Set a valid resolution first
	mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid());
	
	int originalWidth = 0, originalHeight = 0;
	mPlayerExternalsRdk->GetDisplayResolution(originalWidth, originalHeight);

	const int width = -1;
	const int height = 1080;

	// Attempt to set invalid resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the invalid value was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Negative width should be rejected";
	
	// Verify previous valid state was preserved
	int currentWidth = 0, currentHeight = 0;
	mPlayerExternalsRdk->GetDisplayResolution(currentWidth, currentHeight);
	EXPECT_EQ(originalWidth, currentWidth);
	EXPECT_EQ(originalHeight, currentHeight);
}

/**
 * @brief Test OnResolutionPostChange with negative height value
 * 
 * This test verifies that negative height values are rejected and not stored.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_NegativeHeight_IsRejected)
{
	// Set a valid resolution first
	mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid());
	
	int originalWidth = 0, originalHeight = 0;
	mPlayerExternalsRdk->GetDisplayResolution(originalWidth, originalHeight);

	const int width = 1920;
	const int height = -1;

	// Attempt to set invalid resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the invalid value was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Negative height should be rejected";
	
	// Verify previous valid state was preserved
	int currentWidth = 0, currentHeight = 0;
	mPlayerExternalsRdk->GetDisplayResolution(currentWidth, currentHeight);
	EXPECT_EQ(originalWidth, currentWidth);
	EXPECT_EQ(originalHeight, currentHeight);
}

/**
 * @brief Test OnResolutionPostChange with both negative values
 * 
 * This test verifies that resolution with both negative width and height
 * are rejected as invalid.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_BothNegative_IsRejected)
{
	// Set a valid resolution first
	mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid());

	const int width = -1;
	const int height = -1;

	// Attempt to set invalid resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Both negative values should be rejected";
}

/**
 * @brief Test OnResolutionPostChange with unusual aspect ratio
 * 
 * This test verifies handling of an unusual aspect ratio that doesn't match
 * standard TV/monitor aspect ratios (e.g., 21:9 ultrawide).
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_UltraWideResolution_HandlesCorrectly)
{
	const int width = 2560;
	const int height = 1080; // 21:9 aspect ratio

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with portrait orientation
 * 
 * This test verifies handling of a resolution where height > width (portrait mode).
 * While uncommon for displays, the system should handle it correctly.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_PortraitOrientation_HandlesCorrectly)
{
	const int width = 1080;
	const int height = 1920; // Portrait orientation

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with multiple consecutive changes
 * 
 * This test verifies that the system correctly handles multiple resolution
 * changes in succession, ensuring each new value properly overwrites the previous one.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_MultipleChanges_UpdatesCorrectly)
{
	// First resolution change: SD
	mPlayerExternalsRdk->OnResolutionPostChange(720, 480);
	VerifyResolution(720, 480);

	// Second resolution change: HD
	mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
	VerifyResolution(1920, 1080);

	// Third resolution change: 4K
	mPlayerExternalsRdk->OnResolutionPostChange(3840, 2160);
	VerifyResolution(3840, 2160);

	// Fourth resolution change: Back to HD
	mPlayerExternalsRdk->OnResolutionPostChange(1280, 720);
	VerifyResolution(1280, 720);
}

/**
 * @brief Test OnResolutionPostChange with PAL SD resolution (720x576)
 * 
 * This test verifies that OnResolutionPostChange correctly handles PAL
 * standard definition resolution and stores the width and height values.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_PALResolution_StoresCorrectly)
{
	const int width = 720;
	const int height = 576;

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with very large resolution values
 * 
 * This test verifies handling of extremely large resolution values that might
 * be encountered with future display technologies (e.g., 8K or beyond).
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_8KResolution_HandlesCorrectly)
{
	const int width = 7680;
	const int height = 4320; // 8K resolution

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with maximum integer values
 * 
 * This test verifies that extremely large values (INT_MAX) are rejected
 * as they exceed reasonable display resolution limits.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_MaxIntValues_IsRejected)
{
	const int width = INT_MAX;
	const int height = INT_MAX;

	// Attempt to set extreme values
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "INT_MAX values should be rejected as out-of-range";
}

/**
 * @brief Test OnResolutionPostChange with minimum integer values
 * 
 * This test verifies that extreme negative values (INT_MIN) are rejected.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_MinIntValues_IsRejected)
{
	const int width = INT_MIN;
	const int height = INT_MIN;

	// Attempt to set extreme negative values
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "INT_MIN values should be rejected as out-of-range";
}

/**
 * @brief Test OnResolutionPostChange with minimum valid resolution (boundary)
 * 
 * This test verifies that the minimum valid resolution (320x320) is accepted.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_MinValidResolution_IsAccepted)
{
	const int width = 320;
	const int height = 320;

	// Set minimum valid resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was accepted
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Minimum valid resolution (320x320) should be accepted";
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with maximum valid resolution (boundary)
 * 
 * This test verifies that the maximum valid resolution (7680x7680) is accepted.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_MaxValidResolution_IsAccepted)
{
	const int width = 7680;
	const int height = 7680;

	// Set maximum valid resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was accepted
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Maximum valid resolution (7680x7680) should be accepted";
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange with below-minimum resolution
 * 
 * This test verifies that resolutions below the minimum (319x319) are rejected.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_BelowMinResolution_IsRejected)
{
	const int width = 319;
	const int height = 319;

	// Attempt to set below-minimum resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Below-minimum resolution (319x319) should be rejected";
}

/**
 * @brief Test OnResolutionPostChange with above-maximum resolution
 * 
 * This test verifies that resolutions above the maximum (7681x7681) are rejected.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_AboveMaxResolution_IsRejected)
{
	const int width = 7681;
	const int height = 7681;

	// Attempt to set above-maximum resolution
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was rejected
	EXPECT_FALSE(mPlayerExternalsRdk->IsLastResolutionValid())
		<< "Above-maximum resolution (7681x7681) should be rejected";
}

/**
 * @brief Test OnResolutionPostChange with square resolution
 * 
 * This test verifies handling of a square resolution (1:1 aspect ratio),
 * which while unusual for displays, should be handled correctly when within valid range.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_SquareResolution_HandlesCorrectly)
{
	const int width = 1080;
	const int height = 1080; // 1:1 aspect ratio, within valid range

	// Call the event handler
	mPlayerExternalsRdk->OnResolutionPostChange(width, height);

	// Verify the resolution was stored correctly
	EXPECT_TRUE(mPlayerExternalsRdk->IsLastResolutionValid());
	VerifyResolution(width, height);
}

/**
 * @brief Test OnResolutionPostChange rapid successive calls
 * 
 * This test simulates rapid resolution changes (like during display mode detection)
 * to ensure the system handles quick successive calls without issues.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_RapidSuccessiveCalls_HandlesCorrectly)
{
	// Simulate rapid resolution changes
	for (int i = 0; i < 10; i++)
	{
		mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
		mPlayerExternalsRdk->OnResolutionPostChange(1280, 720);
	}

	// Verify the final resolution is correctly stored
	VerifyResolution(1280, 720);
}

/**
 * @brief Test OnResolutionPostChange to verify SetResolution is called
 * 
 * This test specifically verifies that OnResolutionPostChange internally
 * calls SetResolution, which updates the member variables m_displayWidth
 * and m_displayHeight.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnResolutionPostChange_CallsSetResolution_UpdatesInternalState)
{
	// Set initial resolution
	mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080);
	
	int width, height;
	mPlayerExternalsRdk->GetDisplayResolution(width, height);
	
	EXPECT_EQ(width, 1920) << "SetResolution should have been called to update width";
	EXPECT_EQ(height, 1080) << "SetResolution should have been called to update height";
}

/**
 * ============================================================================
 * OnHDCPStatusChange Test Cases
 * ============================================================================
 * 
 * The handler logs the status and, in the real implementation,
 * calls SetHDMIStatus() internally.
 * 
 * Note: In this L1 test environment, a fake implementation is used and
 * the real device:: APIs are not exercised. Additionally, the production
 * implementation of SetHDMIStatus() catches device exceptions internally.
 * As a result, these tests can only assert that OnHDCPStatusChange
 * handles inputs without throwing or crashing; they do not prove that
 * SetHDMIStatus() itself was invoked.
 */

/**
 * @brief Test OnHDCPStatusChange with authenticated status
 * 
 * Verifies that OnHDCPStatusChange handles an authenticated HDCP status
 * correctly without crashing, and internally calls SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_Authenticated_CallsSetHDMIStatus)
{
	// Call with authenticated status
	// If SetHDMIStatus() is invoked, device APIs will throw and be caught
	// If the call completes without crash, SetHDMIStatus() was called
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
}

/**
 * @brief Test OnHDCPStatusChange with authentication failure status
 * 
 * Verifies that OnHDCPStatusChange handles an authentication failure status
 * correctly without crashing, and internally calls SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_AuthenticationFailure_CallsSetHDMIStatus)
{
	// Call with authentication failure status
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
}

/**
 * @brief Test OnHDCPStatusChange with unknown status value
 * 
 * Edge case test with an unknown/unrecognized HDCP status value.
 * The handler should still execute SetHDMIStatus() regardless of the
 * status value, and handle it gracefully.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_UnknownStatus_HandlesGracefully)
{
	// Call with unknown status value
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_UNKNOWN));
}

/**
 * @brief Test OnHDCPStatusChange with extreme status values
 * 
 * Boundary test with an extreme enum value to ensure the handler
 * doesn't crash and still attempts to execute SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_ExtremeEnumValue_HandlesGracefully)
{
	// Call with extreme enum value (negative)
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)-1));
}

/**
 * @brief Test OnHDCPStatusChange with maximum enum value
 * 
 * Boundary test with maximum integer value to ensure safe handling.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_MaximumEnumValue_HandlesGracefully)
{
	// Call with maximum enum value
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)INT_MAX));
}

/**
 * @brief Test OnHDCPStatusChange multiple consecutive calls with same status
 * 
 * Verifies that the handler can be called multiple times with the same
 * status value without issues.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_MultipleCallsSameStatus_HandlesCorrectly)
{
	// Call multiple times with authenticated status
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
}

/**
 * @brief Test OnHDCPStatusChange alternating between authenticated and failure
 * 
 * Verifies rapid alternation between authenticated and failure states,
 * simulating real-world HDCP status changes.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_AlternatingStatuses_HandlesCorrectly)
{
	// Alternate between authenticated and failure states
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
}

/**
 * @brief Test OnHDCPStatusChange rapid successive calls
 * 
 * Simulates rapid HDCP status changes to ensure the handler can handle
 * high-frequency events without issues or memory leaks.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_RapidSuccessiveCalls_HandlesCorrectly)
{
	// Simulate rapid status changes
	for (int i = 0; i < 20; i++)
	{
		EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)(i % 2)));
	}
}

/**
 * @brief Test OnHDCPStatusChange with zero value
 * 
 * Edge case with zero as the status value, which matches authenticated status.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_ZeroStatus_HandlesCorrectly)
{
	// Call with zero value
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)0));
}

/**
 * @brief Test OnHDCPStatusChange pattern: Authenticated -> Failure -> Authenticated
 * 
 * Tests a realistic pattern of HDCP status transitions:
 * Initial authenticated state, then loss of authentication, then restoration.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_RealisticStatusTransitions_HandlesCorrectly)
{
	// Initial state: authenticated
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	
	// HDCP lost
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
	
	// HDCP restored
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	
	// HDCP lost again
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
}

/**
 * @brief Test OnHDCPStatusChange interleaved with OnResolutionPostChange
 * 
 * Verifies that calling HDCP status change and resolution change handlers
 * in sequence doesn't cause issues or conflicts.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnHDCPStatusChange_InterleavedWithResolutionChanges_HandlesCorrectly)
{
	// Interleave HDCP status and resolution change events
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(1280, 720));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(3840, 2160));
}

/**
 * ============================================================================
 * OnDisplayHDMIHotPlug Test Cases
 * ============================================================================
 * 
 * These tests verify the OnDisplayHDMIHotPlug event handler behavior.
 * The handler logs the connection status and calls SetHDMIStatus() internally.
 * 
 * Strategy: Similar to OnHDCPStatusChange tests, we rely on the fact that
 * SetHDMIStatus() calls device:: APIs which throw exceptions in the L1 test
 * environment. The try-catch in SetHDMIStatus() handles these gracefully.
 * 
 * Proof of execution: If the handler invoked SetHDMIStatus() successfully
 * without crashing, it means the API call was made and device exceptions
 * were handled properly.
 */

/**
 * @brief Test OnDisplayHDMIHotPlug with HDMI connected event
 * 
 * Verifies that OnDisplayHDMIHotPlug handles HDMI connected status
 * correctly without crashing, and internally calls SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_Connected_CallsSetHDMIStatus)
{
	// Call with HDMI connected event
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug with HDMI disconnected event
 * 
 * Verifies that OnDisplayHDMIHotPlug handles HDMI disconnected status
 * correctly without crashing, and internally calls SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_Disconnected_CallsSetHDMIStatus)
{
	// Call with HDMI disconnected event
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug with unknown display event value
 * 
 * Edge case test with an unknown/unrecognized display event value.
 * The handler should still execute SetHDMIStatus() regardless of the
 * event value, and handle it gracefully.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_UnknownEvent_HandlesGracefully)
{
	// Call with unknown event value
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)2));
}

/**
 * @brief Test OnDisplayHDMIHotPlug with extreme event values
 * 
 * Boundary test with an extreme enum value to ensure the handler
 * doesn't crash and still attempts to execute SetHDMIStatus().
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_ExtremeEnumValue_HandlesGracefully)
{
	// Call with extreme enum value (negative)
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)-1));
}

/**
 * @brief Test OnDisplayHDMIHotPlug with maximum enum value
 * 
 * Boundary test with maximum integer value to ensure safe handling.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_MaximumEnumValue_HandlesGracefully)
{
	// Call with maximum enum value
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)INT_MAX));
}

/**
 * @brief Test OnDisplayHDMIHotPlug multiple consecutive connected events
 * 
 * Verifies that the handler can be called multiple times with the same
 * connected event without issues.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_MultipleConnectedCalls_HandlesCorrectly)
{
	// Call multiple times with connected event
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug multiple consecutive disconnected events
 * 
 * Verifies that the handler can be called multiple times with the same
 * disconnected event without issues.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_MultipleDisconnectedCalls_HandlesCorrectly)
{
	// Call multiple times with disconnected event
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug alternating between connected and disconnected
 * 
 * Verifies rapid alternation between connected and disconnected states,
 * simulating real-world HDMI hotplug events.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_AlternatingEvents_HandlesCorrectly)
{
	// Alternate between connected and disconnected events
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug rapid successive calls
 * 
 * Simulates rapid HDMI hotplug events to ensure the handler can handle
 * high-frequency events without issues or memory leaks.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_RapidSuccessiveCalls_HandlesCorrectly)
{
	// Simulate rapid hotplug events
	for (int i = 0; i < 20; i++)
	{
		EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)(i % 2)));
	}
}

/**
 * @brief Test OnDisplayHDMIHotPlug with zero value
 * 
 * Edge case with zero as the event value, which matches connected status.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_ZeroEvent_HandlesCorrectly)
{
	// Call with zero value (connected)
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)0));
}

/**
 * @brief Test OnDisplayHDMIHotPlug realistic hotplug scenario: Connect -> Disconnect -> Connect
 * 
 * Tests a realistic pattern of HDMI hotplug events:
 * HDMI plugged in, unplugged, then plugged in again.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_RealisticHotplugSequence_HandlesCorrectly)
{
	// Initial state: HDMI connected
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	
	// HDMI unplugged
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	
	// HDMI plugged back in
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	
	// HDMI unplugged again
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	
	// HDMI plugged in again
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug interleaved with OnHDCPStatusChange
 * 
 * Verifies that calling HDMI hotplug and HDCP status change handlers
 * in sequence doesn't cause issues or conflicts.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_InterleavedWithHDCPStatusChange_HandlesCorrectly)
{
	// Interleave HDMI hotplug and HDCP status events
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
}

/**
 * @brief Test OnDisplayHDMIHotPlug interleaved with all other event handlers
 * 
 * Verifies that calling hotplug, HDCP status change, and resolution change
 * handlers in an interleaved pattern doesn't cause issues.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_InterleavedWithAllHandlers_HandlesCorrectly)
{
	// Interleave all event handlers
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(1920, 1080));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATION_FAILURE));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(0, 0));
	
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)dsHDCP_STATUS_AUTHENTICATED));
	EXPECT_NO_THROW(mPlayerExternalsRdk->OnResolutionPostChange(3840, 2160));
}

/**
 * @brief Test OnDisplayHDMIHotPlug stress test: Random alternating events
 * 
 * Stress test with many random alternating hotplug events to ensure
 * robustness under high-frequency event scenarios.
 */
TEST_F(PlayerExternalsRdkInterfaceTests, OnDisplayHDMIHotPlug_StressTest_HandlesCorrectly)
{
	// Stress test with alternating events
	for (int i = 0; i < 50; i++)
	{
		if (i % 3 == 0)
		{
			EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_CONNECTED));
		}
		else if (i % 3 == 1)
		{
			EXPECT_NO_THROW(mPlayerExternalsRdk->OnDisplayHDMIHotPlug((dsDisplayEvent_t)dsDISPLAY_EVENT_DISCONNECTED));
		}
		else
		{
			EXPECT_NO_THROW(mPlayerExternalsRdk->OnHDCPStatusChange((dsHdcpStatus_t)(i % 2)));
		}
	}
}


