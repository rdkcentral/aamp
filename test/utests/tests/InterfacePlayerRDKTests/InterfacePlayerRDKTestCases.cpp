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
	InterfacePlayerRDK *mPlayer;
	InterfacePlayerPriv *mPrivatePlayer;
	guint mCapturedTimerId;
	GSourceFunc mCapturedTimerFunc;
	gpointer mCapturedUserData;
	
	void SetUp() override
	{
		// Setup mocks
		g_mockGLib = new NiceMock<MockGLib>();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockPlayerUtils = new NiceMock<MockPlayerUtils>();
		
		// Create player instance
		mPlayer = new InterfacePlayerRDK();
		
		// Get private player context
		mPrivatePlayer = mPlayer->GetPrivatePlayer();
		
		// Initialize config parameters if needed
		if (!mPlayer->m_gstConfigParam)
		{
			mPlayer->m_gstConfigParam = new Configs();
			mPlayer->m_gstConfigParam->progressTimer = 1.0; // 1 second default
			mPlayer->m_gstConfigParam->monitorAV = false;
		}
		
		// Initialize capture variables
		mCapturedTimerId = 0;
		mCapturedTimerFunc = nullptr;
		mCapturedUserData = nullptr;
	}
	
	void TearDown() override
	{
		// Clean up player
		if (mPlayer)
		{
			if (mPlayer->m_gstConfigParam)
			{
				delete mPlayer->m_gstConfigParam;
				mPlayer->m_gstConfigParam = nullptr;
			}
			delete mPlayer;
			mPlayer = nullptr;
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
				mCapturedTimerFunc = function;
				mCapturedUserData = data;
				return ++mCapturedTimerId;
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
	mPlayer->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	// Mock g_source_remove to avoid actual GLib calls
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Act: Call IdleCallback - should not crash even with nullptr callback
	gboolean result = InterfacePlayerRDK::IdleCallback(mPlayer);
	
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
	mPlayer->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	SetupTimerMocks();
	
	// Mock g_source_remove
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Expect that g_timeout_add_full is NOT called (no timer should be added)
	EXPECT_CALL(*g_mockGLib, g_timeout_add_full(_, _, _, _, _))
		.Times(0);
	
	// Act: Call IdleCallback
	gboolean result = InterfacePlayerRDK::IdleCallback(mPlayer);
	
	// Assert: Timer function should not be captured (remains null)
	EXPECT_EQ(mCapturedTimerFunc, nullptr);
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
	// Arrange: Set a valid progressCb
	bool callbackInvoked = false;
	mPlayer->callbackMap[InterfaceCB::progressCb] = [&callbackInvoked]() {
		callbackInvoked = true;
	};
	
	SetupTimerMocks();
	
	// Mock g_source_remove to return false (no timer currently running)
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Expect that timer is added exactly once
	EXPECT_CALL(*g_mockGLib, g_timeout_add_full(_, _, NotNull(), mPlayer, _))
		.Times(1)
		.WillOnce(Return(123)); // Return a fake timer ID
	
	// Act: Call IdleCallback
	gboolean result = InterfacePlayerRDK::IdleCallback(mPlayer);
	
	// Assert: Timer function should be captured
	EXPECT_NE(mCapturedTimerFunc, nullptr);
	EXPECT_EQ(mCapturedUserData, mPlayer);
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
	mPlayer->callbackMap[InterfaceCB::progressCb] = nullptr;
	
	// Disable AV monitoring to simplify test
	mPlayer->m_gstConfigParam->monitorAV = false;
	
	// Act: Call ProgressCallbackOnTimeout - should not crash
	gboolean result = InterfacePlayerRDK::ProgressCallbackOnTimeout(mPlayer);
	
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
	
	mPlayer->callbackMap[InterfaceCB::progressCb] = [&progressCalled]() {
		progressCalled = true;
	};
	mPlayer->callbackMap[InterfaceCB::idleCb] = [&idleCalled]() {
		idleCalled = true;
	};
	
	// Simulate callbacks being unregistered (as would happen in UnregisterFirstFrameCallbacks)
	mPlayer->callbackMap[InterfaceCB::progressCb] = nullptr;
	mPlayer->callbackMap[InterfaceCB::idleCb] = nullptr;
	
	// Disable AV monitoring
	mPlayer->m_gstConfigParam->monitorAV = false;
	
	// Mock timer operations
	EXPECT_CALL(*g_mockGLib, g_source_remove(_))
		.WillRepeatedly(Return(FALSE));
	
	// Act: Trigger callbacks that might have been scheduled before unregistration
	// This should not crash even though callbacks are now nullptr
	gboolean idleResult = InterfacePlayerRDK::IdleCallback(mPlayer);
	gboolean progressResult = InterfacePlayerRDK::ProgressCallbackOnTimeout(mPlayer);
	
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
