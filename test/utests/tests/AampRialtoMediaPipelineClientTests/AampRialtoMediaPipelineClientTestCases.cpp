/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file AampRialtoMediaPipelineClientTestCases.cpp
 * @brief L1 unit tests for AampRialtoMediaPipelineClient.
 *
 * Tests are structured per the TDD implementation plan in
 * docs/rialto-integration/aamp-rialto-player-analysis.md — Phase 1.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>
#include <future>
#include <atomic>
#include <memory>
#include <thread>

#include "AampRialtoMediaPipelineClient.h"

using ::testing::_;
using ::testing::MockFunction;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class AampRialtoMediaPipelineClientTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_client = std::make_unique<AampRialtoMediaPipelineClient>();
	}

	void TearDown() override
	{
		m_client.reset();
	}

	std::unique_ptr<AampRialtoMediaPipelineClient> m_client;
};

// ---------------------------------------------------------------------------
// Phase 1 — callback baseline tests
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyNeedMediaData_WithCallback_InvokesCallback)
{
	bool called = false;
	int32_t gotSource = -1;
	size_t  gotFrameCount = 0;
	uint32_t gotRequestId = 0;

	m_client->SetNeedDataCallback(
		[&](int32_t src, size_t fc, uint32_t rid) {
			called = true;
			gotSource    = src;
			gotFrameCount = fc;
			gotRequestId = rid;
		});

	m_client->notifyNeedMediaData(
		/*sourceId=*/1,
		/*frameCount=*/10,
		/*needDataRequestId=*/42,
		/*mediaPlayerShmInfo=*/nullptr);

	EXPECT_TRUE(called);
	EXPECT_EQ(gotSource, 1);
	EXPECT_EQ(gotFrameCount, 10u);
	EXPECT_EQ(gotRequestId, 42u);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyNeedMediaData_WithoutCallback_DoesNotCrash)
{
	// No callback installed — must not crash.
	EXPECT_NO_THROW(m_client->notifyNeedMediaData(1, 10, 42, nullptr));
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyCancelNeedMediaData_WithCallback_InvokesCallback)
{
	bool called = false;
	int32_t gotSource = -1;

	m_client->SetCancelNeedDataCallback(
		[&](int32_t src) {
			called    = true;
			gotSource = src;
		});

	m_client->notifyCancelNeedMediaData(/*sourceId=*/2);

	EXPECT_TRUE(called);
	EXPECT_EQ(gotSource, 2);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyCancelNeedMediaData_WithoutCallback_DoesNotCrash)
{
	EXPECT_NO_THROW(m_client->notifyCancelNeedMediaData(2));
}

TEST_F(AampRialtoMediaPipelineClientTest,
	SetNeedDataCallback_ReplacesExisting)
{
	int callCount = 0;

	// Install first callback.
	m_client->SetNeedDataCallback([&](int32_t, size_t, uint32_t) {
		callCount++;
	});

	// Override with second callback.
	m_client->SetNeedDataCallback([&](int32_t, size_t, uint32_t) {
		callCount += 10; // distinct increment so we know which fired
	});

	m_client->notifyNeedMediaData(1, 1, 1, nullptr);

	// Only the second callback (increment of 10) should have fired.
	EXPECT_EQ(callCount, 10);
}

TEST_F(AampRialtoMediaPipelineClientTest, ClearCallbacks_RemovesInstalledCallbacks)
{
	int callCount = 0;
	m_client->SetNeedDataCallback([&](int32_t, size_t, uint32_t) { callCount++; });
	m_client->SetPlaybackStateCallback(
		[&](firebolt::rialto::PlaybackState) { callCount++; });
	m_client->SetBufferUnderflowCallback([&](int32_t) { callCount++; });
	m_client->SetPlaybackErrorCallback(
		[&](int32_t, firebolt::rialto::PlaybackError) { callCount++; });

	m_client->ClearCallbacks();

	m_client->notifyNeedMediaData(1, 1, 1, nullptr);
	m_client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
	m_client->notifyBufferUnderflow(1);
	m_client->notifyPlaybackError(1, firebolt::rialto::PlaybackError::UNKNOWN);

	EXPECT_EQ(callCount, 0);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	AllOtherNotifications_DoNotCrash)
{
	// None of these should crash with default (stub) implementations.
	EXPECT_NO_THROW(m_client->notifyPlaybackState(
		firebolt::rialto::PlaybackState::PLAYING));
	EXPECT_NO_THROW(m_client->notifyPosition(0));
	EXPECT_NO_THROW(m_client->notifyDuration(0));
	EXPECT_NO_THROW(m_client->notifyNativeSize(1280, 720));
	EXPECT_NO_THROW(m_client->notifyVideoData(true));
	EXPECT_NO_THROW(m_client->notifyAudioData(true));
	EXPECT_NO_THROW(m_client->notifyNetworkState(
		firebolt::rialto::NetworkState::IDLE));
	EXPECT_NO_THROW(m_client->notifyBufferUnderflow(0));
	EXPECT_NO_THROW(m_client->notifyPlaybackError(
		0, firebolt::rialto::PlaybackError::DECRYPTION));
	EXPECT_NO_THROW(m_client->notifySourceFlushed(0));
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyBufferUnderflow_WithCallback_InvokesCallback)
{
	bool called = false;
	int32_t gotSource = -1;

	m_client->SetBufferUnderflowCallback(
		[&](int32_t src) {
			called    = true;
			gotSource = src;
		});

	m_client->notifyBufferUnderflow(/*sourceId=*/2);

	EXPECT_TRUE(called);
	EXPECT_EQ(gotSource, 2);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyBufferUnderflow_WithoutCallback_DoesNotCrash)
{
	// No callback installed — must not crash.
	EXPECT_NO_THROW(m_client->notifyBufferUnderflow(2));
}

TEST_F(AampRialtoMediaPipelineClientTest,
	SetBufferUnderflowCallback_ReplacesExisting)
{
	int callCount = 0;

	m_client->SetBufferUnderflowCallback([&](int32_t) { callCount++;     });
	m_client->SetBufferUnderflowCallback([&](int32_t) { callCount += 10; });

	m_client->notifyBufferUnderflow(1);

	// Only the second callback should fire.
	EXPECT_EQ(callCount, 10);
}

// ---------------------------------------------------------------------------
// notifyPlaybackError — PlaybackErrorCallback baseline tests
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyPlaybackError_WithCallback_InvokesCallback)
{
	bool called = false;
	int32_t gotSource = -1;
	firebolt::rialto::PlaybackError gotError =
		firebolt::rialto::PlaybackError::UNKNOWN;

	m_client->SetPlaybackErrorCallback(
		[&](int32_t src, firebolt::rialto::PlaybackError error) {
			called    = true;
			gotSource = src;
			gotError  = error;
		});

	m_client->notifyPlaybackError(
		/*sourceId=*/3, firebolt::rialto::PlaybackError::OUTPUT_PROTECTION);

	EXPECT_TRUE(called);
	EXPECT_EQ(gotSource, 3);
	EXPECT_EQ(gotError, firebolt::rialto::PlaybackError::OUTPUT_PROTECTION);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyPlaybackError_WithoutCallback_DoesNotCrash)
{
	// No callback installed — must not crash.
	EXPECT_NO_THROW(m_client->notifyPlaybackError(
		2, firebolt::rialto::PlaybackError::DECRYPTION));
}

TEST_F(AampRialtoMediaPipelineClientTest,
	SetPlaybackErrorCallback_ReplacesExisting)
{
	int callCount = 0;

	m_client->SetPlaybackErrorCallback(
		[&](int32_t, firebolt::rialto::PlaybackError) { callCount++; });
	m_client->SetPlaybackErrorCallback(
		[&](int32_t, firebolt::rialto::PlaybackError) { callCount += 10; });

	m_client->notifyPlaybackError(1, firebolt::rialto::PlaybackError::UNKNOWN);

	// Only the second callback should fire.
	EXPECT_EQ(callCount, 10);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	Callbacks_ConcurrentInstallAndDelivery_OnlyRunsWholeCallbacks)
{
	/**
	 * @brief Regression (M8): the callbacks are std::function members that
	 *        Set*Callback()/ClearCallbacks() assign on the control thread
	 *        while Rialto delivers events on its own thread.  The capture is
	 *        large enough to be heap allocated, so an unguarded copy or
	 *        assignment can run a half-replaced callable.  Under a thread
	 *        sanitizer the unguarded members are also reported as a race.
	 */
	std::atomic<bool> done{false};
	std::atomic<int> calls{0};
	std::atomic<bool> corrupted{false};

	std::thread deliverer([&] {
		while (!done.load())
		{
			m_client->notifyNeedMediaData(1, 1, 1, nullptr);
			m_client->notifyPlaybackState(firebolt::rialto::PlaybackState::PLAYING);
		}
	});

	for (int i = 0; i < 2000; ++i)
	{
		const auto guard = std::make_shared<std::atomic<int>>(0x5A5A);
		std::array<int, 16> padding{};
		padding.fill(0x5A5A);
		m_client->SetNeedDataCallback(
			[&, guard, padding](int32_t, size_t, uint32_t) {
				if (guard->load() != 0x5A5A || padding[15] != 0x5A5A)
				{
					corrupted = true;
				}
				++calls;
			});
		m_client->SetPlaybackStateCallback(
			[&, guard](firebolt::rialto::PlaybackState) {
				if (guard->load() != 0x5A5A)
				{
					corrupted = true;
				}
			});
		if ((i % 7) == 0)
		{
			m_client->ClearCallbacks();
		}
	}
	done = true;
	deliverer.join();

	EXPECT_FALSE(corrupted.load());
}

TEST_F(AampRialtoMediaPipelineClientTest,
	Callback_MayReplaceItselfWhileRunning_DoesNotDeadlock)
{
	/**
	 * @brief The callback is invoked without the client's lock, so a callback
	 *        that installs another callback (or clears them) must not
	 *        deadlock.
	 */
	int first = 0;
	int second = 0;
	m_client->SetNeedDataCallback([&](int32_t, size_t, uint32_t) {
		++first;
		m_client->SetNeedDataCallback(
			[&](int32_t, size_t, uint32_t) { ++second; });
	});

	auto result = std::async(std::launch::async, [&] {
		m_client->notifyNeedMediaData(1, 1, 1, nullptr);
		m_client->notifyNeedMediaData(1, 1, 2, nullptr);
	});
	ASSERT_EQ(result.wait_for(std::chrono::seconds(5)),
		std::future_status::ready);
	EXPECT_EQ(first, 1);
	EXPECT_EQ(second, 1);
}

// ---------------------------------------------------------------------------
// Log levels: frequent events stay out of INFO, rare ones keep their value
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMediaPipelineClientTest,
	FrequentNotifications_DoNotLogAtInfo)
{
	/**
	 * @brief Position, need-data, cancel-need-data and QoS arrive many times a
	 *        second, so they must not appear at INFO.
	 */
	testing::internal::CaptureStdout();
	m_client->notifyPosition(1'000'000'000LL);
	m_client->notifyNeedMediaData(1, 24, 7, nullptr);
	m_client->notifyCancelNeedMediaData(1);
	m_client->notifyQos(1, firebolt::rialto::QosInfo{});
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_EQ(log.find("[INFO]"), std::string::npos) << log;
}

TEST_F(AampRialtoMediaPipelineClientTest,
	notifyDuration_LogsTheDurationAtInfo)
{
	/**
	 * @brief The duration rarely changes, so the value stays visible at INFO.
	 */
	testing::internal::CaptureStdout();
	m_client->notifyDuration(123'456'789LL);
	const std::string log = testing::internal::GetCapturedStdout();

	EXPECT_NE(log.find("[INFO]"), std::string::npos) << log;
	EXPECT_NE(log.find("123456789"), std::string::npos) << log;
}
