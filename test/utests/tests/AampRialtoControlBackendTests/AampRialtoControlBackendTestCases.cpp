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

/**
 * @file AampRialtoControlBackendTestCases.cpp
 * @brief L1 unit tests for AampRialtoControlBackend.
 *
 * Tests verify:
 *   - Constructor registers with IControl
 *   - waitForRunning() returns immediately when already RUNNING
 *   - waitForRunning() blocks and returns true when state becomes RUNNING
 *   - waitForRunning() returns false on timeout
 *   - Graceful handling of null factory / null control
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>

#include "AampRialtoControlBackend.h"
#include "MockIControlFactory.h"

using ::testing::_;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class AampRialtoControlBackendTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockControlFactory = std::make_shared<MockIControlFactory>();
		m_mockControl = std::make_shared<MockIControl>();
	}

	void TearDown() override
	{
		m_mockControl.reset();
		g_mockControlFactory.reset();
	}

	std::shared_ptr<MockIControl> m_mockControl;
};

// ===========================================================================
// Constructor tests
// ===========================================================================

TEST_F(AampRialtoControlBackendTest,
	Constructor_NullFactory_DoesNotCrash)
{
	g_mockControlFactory.reset();
	// IControlFactory::createFactory() returns nullptr — constructor
	// should handle this gracefully without crashing.
	AampRialtoControlBackend backend;
	// waitForRunning should return false immediately (no control registered)
	EXPECT_FALSE(backend.waitForRunning(10));
}

TEST_F(AampRialtoControlBackendTest,
	Constructor_FactoryReturnsNullControl_DoesNotCrash)
{
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(nullptr));

	AampRialtoControlBackend backend;
	EXPECT_FALSE(backend.waitForRunning(10));
}

TEST_F(AampRialtoControlBackendTest,
	Constructor_RegisterClientFails_DoesNotCrash)
{
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(Return(false));

	AampRialtoControlBackend backend;
	EXPECT_FALSE(backend.waitForRunning(10));
}

TEST_F(AampRialtoControlBackendTest,
	Constructor_RegisterClientSucceeds_InitialStateUnknown)
{
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(DoAll(
			SetArgReferee<1>(firebolt::rialto::ApplicationState::UNKNOWN),
			Return(true)));

	AampRialtoControlBackend backend;
	// State is UNKNOWN — waitForRunning with short timeout should return false
	EXPECT_FALSE(backend.waitForRunning(10));
}

// ===========================================================================
// waitForRunning tests
// ===========================================================================

TEST_F(AampRialtoControlBackendTest,
	WaitForRunning_AlreadyRunning_ReturnsImmediately)
{
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(DoAll(
			SetArgReferee<1>(firebolt::rialto::ApplicationState::RUNNING),
			Return(true)));

	AampRialtoControlBackend backend;
	EXPECT_TRUE(backend.waitForRunning(5000));
}

TEST_F(AampRialtoControlBackendTest,
	WaitForRunning_StateChangesToRunning_ReturnsTrue)
{
	std::weak_ptr<firebolt::rialto::IControlClient> capturedClient;

	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(Invoke(
			[&capturedClient](
				std::weak_ptr<firebolt::rialto::IControlClient> client,
				firebolt::rialto::ApplicationState &appState)
			{
				capturedClient = client;
				appState = firebolt::rialto::ApplicationState::UNKNOWN;
				return true;
			}));

	AampRialtoControlBackend backend;

	// Notify state change from a different thread after a short delay.
	std::thread notifier([&capturedClient]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		auto client = capturedClient.lock();
		ASSERT_NE(client, nullptr);
		client->notifyApplicationState(
			firebolt::rialto::ApplicationState::RUNNING);
	});

	EXPECT_TRUE(backend.waitForRunning(2000));
	notifier.join();
}

TEST_F(AampRialtoControlBackendTest,
	WaitForRunning_Timeout_ReturnsFalse)
{
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(DoAll(
			SetArgReferee<1>(firebolt::rialto::ApplicationState::UNKNOWN),
			Return(true)));

	AampRialtoControlBackend backend;

	auto start = std::chrono::steady_clock::now();
	EXPECT_FALSE(backend.waitForRunning(50));
	auto elapsed = std::chrono::steady_clock::now() - start;
	// Should have waited approximately 50ms (allow some slack).
	EXPECT_GE(elapsed, std::chrono::milliseconds(40));
}

TEST_F(AampRialtoControlBackendTest,
	NotificationAfterBackendDestroyed_DoesNotTouchFreedBackend)
{
	/**
	 * @brief Regression (M13): the client held a reference to the backend, and
	 *        the backend's mutex and condition variable were destroyed before
	 *        the IControl that can still deliver a notification.  A
	 *        notification arriving after the backend is gone must be harmless.
	 *        Run under AddressSanitizer to see the old fault.
	 */
	std::weak_ptr<firebolt::rialto::IControlClient> capturedClient;
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(Invoke(
			[&capturedClient](
				std::weak_ptr<firebolt::rialto::IControlClient> client,
				firebolt::rialto::ApplicationState &appState)
			{
				capturedClient = client;
				appState = firebolt::rialto::ApplicationState::UNKNOWN;
				return true;
			}));

	auto backend = std::make_unique<AampRialtoControlBackend>();
	// Rialto's event thread holds the client for the length of a notification.
	auto heldClient = capturedClient.lock();
	ASSERT_NE(heldClient, nullptr);

	backend.reset();
	m_mockControl.reset();

	heldClient->notifyApplicationState(firebolt::rialto::ApplicationState::RUNNING);
}

TEST_F(AampRialtoControlBackendTest,
	RegisterClient_NotificationDuringRegistration_IsNotOverwritten)
{
	/**
	 * @brief Regression (M13): registerClient() reports the state at the time
	 *        it was called and could be delivered after a newer notification;
	 *        the stale value must not replace it.
	 */
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(Invoke(
			[](std::weak_ptr<firebolt::rialto::IControlClient> client,
			   firebolt::rialto::ApplicationState &appState)
			{
				// Rialto's thread reports RUNNING while registerClient() is
				// still returning the older UNKNOWN.
				if (auto c = client.lock())
				{
					c->notifyApplicationState(
						firebolt::rialto::ApplicationState::RUNNING);
				}
				appState = firebolt::rialto::ApplicationState::UNKNOWN;
				return true;
			}));

	AampRialtoControlBackend backend;
	EXPECT_TRUE(backend.waitForRunning(50));
}

TEST_F(AampRialtoControlBackendTest,
	RegisterClient_ConcurrentNotification_IsRaceFree)
{
	/**
	 * @brief Regression (M13): registerClient() wrote the state member that a
	 *        notification writes under the lock, with no lock.  Under a thread
	 *        sanitizer the old code is reported as a data race.
	 */
	EXPECT_CALL(*g_mockControlFactory, createControl())
		.WillOnce(Return(m_mockControl));
	std::thread notifier;
	EXPECT_CALL(*m_mockControl, registerClient(_, _))
		.WillOnce(Invoke(
			[&notifier](std::weak_ptr<firebolt::rialto::IControlClient> client,
			            firebolt::rialto::ApplicationState &appState)
			{
				notifier = std::thread([client]() {
					if (auto c = client.lock())
					{
						c->notifyApplicationState(
							firebolt::rialto::ApplicationState::RUNNING);
					}
				});
				appState = firebolt::rialto::ApplicationState::UNKNOWN;
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				return true;
			}));

	AampRialtoControlBackend backend;
	notifier.join();
	EXPECT_TRUE(backend.waitForRunning(2000));
}
