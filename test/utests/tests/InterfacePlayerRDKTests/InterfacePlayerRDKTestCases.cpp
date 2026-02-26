/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include "middleware/InterfacePlayerRDK.h"
#include "middleware/InterfacePlayerPriv.h"
#include "MockGLib.h"
#include "MockGStreamer.h"
#include "MockAampConfig.h"
#include "MockPlayerUtils.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::NotNull;
using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::Invoke;

/**
 * @brief Test fixture for InterfacePlayerRDK callback tests
 * 
 * This fixture tests the null pointer safety fixes in IdleCallback and
 * ProgressCallbackOnTimeout functions, ensuring the player doesn't crash
 * when callbacks are unregistered.
 */
class InterfacePlayerRDKCallbackTest : public ::testing::Test
{
protected:
	InterfacePlayerRDK *m_player;
	guint capturedTimerId;
	GSourceFunc capturedTimerFunc;
	gpointer capturedUserData;
	
	void SetUp() override
	{
		// Setup mocks
		g_mockGLib = new NiceMock<MockGLib>();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockPlayerUtils = new NiceMock<MockPlayerUtils>();
		
		// Create player instance
		m_player = new InterfacePlayerRDK();
		
		// Initialize config parameters for test
		// Note: m_gstConfigParam is always null after construction, so we allocate it
		m_player->m_gstConfigParam = new Configs();
		m_player->m_gstConfigParam->progressTimer = 1.0; // 1 second default
		m_player->m_gstConfigParam->monitorAV = false;
		
		// Initialize capture variables
		capturedTimerId = 0;
		capturedTimerFunc = nullptr;
		capturedUserData = nullptr;
	}
	
	void TearDown() override
	{
		// Clean up player
		// Note: We delete m_gstConfigParam because we allocated it in SetUp
		if (m_player)
		{
			if (m_player->m_gstConfigParam)
			{
				delete m_player->m_gstConfigParam;
				m_player->m_gstConfigParam = nullptr;
			}
			delete m_player;
			m_player = nullptr;
		}
		
		// Clean up mocks
		delete g_mockPlayerUtils;
		g_mockPlayerUtils = nullptr;
		
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
		
		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;
		
		delete g_mockGLib;
		g_mockGLib = nullptr;
	}
	
	/**
	 * @brief Helper to setup mocks for timer operations
	 */
	void SetupTimerMocks()
	{
		// Mock g_timeout_add_full to capture timer registration
		ON_CALL(*g_mockGLib, g_timeout_add_full(_, _, _, _, _))
			.WillByDefault(Invoke([this](gint priority, guint interval, 
				GSourceFunc function, gpointer data, GDestroyNotify notify) -> guint
			{
				capturedTimerFunc = function;
				capturedUserData = data;
				// Note: GDestroyNotify parameter is intentionally ignored in tests
				// as we're only verifying timer setup, not cleanup behavior
				return ++capturedTimerId;
			}));
	}
};

/**
 * @brief Test that IdleCallback doesn't crash when progressCb is nullptr
 * 
 * VPLAY-12819: This test verifies the fix for the crash that occurred when
 * IdleCallback was triggered after callbacks were unregistered.
 */
TEST_F(InterfacePlayerRDKCallbackTest, IdleCallback_NullProgressCb_DoesNotCrash)
{
	// Arrange: Set progressCb to nullptr (simulating unregistered callback)
	m_player->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	// Mock g_source_remove to avoid actual GLib calls
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Act: Call IdleCallback - should not crash even with nullptr callback
	gboolean result = InterfacePlayerRDK::IdleCallback(m_player);
	
	// Assert: Should return G_SOURCE_REMOVE
	EXPECT_EQ(result, G_SOURCE_REMOVE);
}

/**
 * @brief Test that IdleCallback doesn't set up periodic timer when progressCb is nullptr
 * 
 * This ensures that when the progress callback is not registered, the periodic
 * timer for progress updates is not started, preventing unnecessary timer callbacks.
 */
TEST_F(InterfacePlayerRDKCallbackTest, IdleCallback_NullProgressCb_DoesNotSetupTimer)
{
	// Arrange: Set progressCb to nullptr
	m_player->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	SetupTimerMocks();
	
	// Mock g_source_remove
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Expect that g_timeout_add_full is NOT called (no timer should be added)
	EXPECT_CALL(*g_mockGLib, g_timeout_add_full(_, _, _, _, _))
		.Times(0);
	
	// Act: Call IdleCallback
	gboolean result = InterfacePlayerRDK::IdleCallback(m_player);
	
	// Assert: Timer function should not be captured (remains null)
	EXPECT_EQ(capturedTimerFunc, nullptr);
	EXPECT_EQ(result, G_SOURCE_REMOVE);
}

/**
 * @brief Test that IdleCallback sets up timer when progressCb is valid
 * 
 * This is the positive test case - when progress callback is registered,
 * the periodic timer should be set up correctly.
 */
TEST_F(InterfacePlayerRDKCallbackTest, IdleCallback_ValidProgressCb_SetupsTimer)
{
	// Arrange: Set a valid progressCb (empty for this test as we only verify timer setup)
	m_player->callbackMap[InterfaceCB::progressCb] = []() {
		// Empty callback - this test only verifies timer setup, not callback execution
	};
	
	SetupTimerMocks();
	
	// Mock g_source_remove to return false (no timer currently running)
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Expect that timer is added exactly once
	EXPECT_CALL(*g_mockGLib, g_timeout_add_full(_, _, NotNull(), m_player, _))
		.Times(1)
		.WillOnce(Return(123)); // Return a fake timer ID
	
	// Act: Call IdleCallback
	gboolean result = InterfacePlayerRDK::IdleCallback(m_player);
	
	// Assert: Timer function should be captured
	EXPECT_NE(capturedTimerFunc, nullptr);
	EXPECT_EQ(capturedUserData, m_player);
	EXPECT_EQ(result, G_SOURCE_REMOVE);
}

/**
 * @brief Test that ProgressCallbackOnTimeout doesn't crash when progressCb is nullptr
 * 
 * This test ensures that even if the periodic timer fires after callbacks have been
 * unregistered, the player doesn't crash when trying to invoke the progress callback.
 */
TEST_F(InterfacePlayerRDKCallbackTest, ProgressCallbackOnTimeout_NullProgressCb_DoesNotCrash)
{
	// Arrange: Set progressCb to nullptr (simulating unregistered callback)
	m_player->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	// Disable AV monitoring to simplify test
	m_player->m_gstConfigParam->monitorAV = false;
	
	// Act: Call ProgressCallbackOnTimeout - should not crash
	gboolean result = InterfacePlayerRDK::ProgressCallbackOnTimeout(m_player);
	
	// Assert: Should return G_SOURCE_CONTINUE (periodic timer continues)
	EXPECT_EQ(result, G_SOURCE_CONTINUE);
}

/**
 * @brief Test the scenario where callbacks are unregistered and then callbacks are triggered
 * 
 * This simulates the real-world scenario described in VPLAY-12819 where:
 * 1. Callbacks are registered initially
 * 2. UnregisterFirstFrameCallbacks is called (sets callbacks to nullptr)
 * 3. Pending idle callbacks or timers fire
 * 
 * The player should handle this gracefully without crashing.
 */
TEST_F(InterfacePlayerRDKCallbackTest, CallbacksUnregistered_ThenTriggered_DoesNotCrash)
{
	// Arrange: Initially register callbacks
	bool progressCalled = false;
	bool idleCalled = false;
	
	m_player->callbackMap[InterfaceCB::progressCb] = [&progressCalled]() {
		progressCalled = true;
	};
	m_player->callbackMap[InterfaceCB::idleCb] = [&idleCalled]() {
		idleCalled = true;
	};
	
	// Simulate callbacks being unregistered (as would happen in UnregisterFirstFrameCallbacks)
	m_player->callbackMap[InterfaceCB::progressCb] = nullptr;
	m_player->callbackMap[InterfaceCB::idleCb] = nullptr;
	
	// Disable AV monitoring
	m_player->m_gstConfigParam->monitorAV = false;
	
	// Mock timer operations
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Act: Trigger callbacks that might have been scheduled before unregistration
	// This should not crash even though callbacks are now nullptr
	gboolean idleResult = InterfacePlayerRDK::IdleCallback(m_player);
	gboolean progressResult = InterfacePlayerRDK::ProgressCallbackOnTimeout(m_player);
	
	// Assert: Both should return without crashing
	EXPECT_EQ(idleResult, G_SOURCE_REMOVE);
	EXPECT_EQ(progressResult, G_SOURCE_CONTINUE);
	
	// Callbacks should not have been invoked since they were nullptr
	EXPECT_FALSE(progressCalled);
	EXPECT_FALSE(idleCalled);
}

/**
 * @brief Test that IdleCallback handles null player pointer gracefully
 * 
 * Edge case: ensure nullptr player doesn't cause crash.
 */
TEST_F(InterfacePlayerRDKCallbackTest, IdleCallback_NullPlayer_DoesNotCrash)
{
	// Act: Call with nullptr player
	gboolean result = InterfacePlayerRDK::IdleCallback(nullptr);
	
	// Assert: Should return G_SOURCE_REMOVE safely
	EXPECT_EQ(result, G_SOURCE_REMOVE);
}

/**
 * @brief Test that ProgressCallbackOnTimeout handles null player pointer gracefully
 * 
 * Edge case: ensure nullptr player doesn't cause crash.
 */
TEST_F(InterfacePlayerRDKCallbackTest, ProgressCallbackOnTimeout_NullPlayer_DoesNotCrash)
{
	// Act: Call with nullptr player
	gboolean result = InterfacePlayerRDK::ProgressCallbackOnTimeout(nullptr);
	
	// Assert: Should return G_SOURCE_CONTINUE safely
	EXPECT_EQ(result, G_SOURCE_CONTINUE);
}
