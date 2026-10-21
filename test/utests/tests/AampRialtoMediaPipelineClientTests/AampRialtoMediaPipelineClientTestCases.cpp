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

#include <atomic>
#include <future>
#include <memory>
#include <thread>

#include "AampRialtoMediaPipelineClient.h"

using ::testing::_;
using ::testing::StrictMock;
using firebolt::rialto::PlaybackError;
using firebolt::rialto::PlaybackState;

class MockPipelineEvents : public IAampRialtoPipelineEvents
{
public:
	MOCK_METHOD(void, OnNeedMediaData, (int32_t, size_t, uint32_t), (override));
	MOCK_METHOD(void, OnCancelNeedMediaData, (int32_t), (override));
	MOCK_METHOD(void, OnPlaybackState, (PlaybackState), (override));
	MOCK_METHOD(void, OnPosition, (int64_t), (override));
	MOCK_METHOD(void, OnDuration, (int64_t), (override));
	MOCK_METHOD(void, OnBufferUnderflow, (int32_t), (override));
	MOCK_METHOD(void, OnPlaybackError, (int32_t, PlaybackError), (override));
};

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
	StrictMock<MockPipelineEvents> m_events;
};

// ---------------------------------------------------------------------------
// Each forwarded event reaches the sink with its arguments
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMediaPipelineClientTest, notifyNeedMediaData_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnNeedMediaData(1, 10u, 42u));
	m_client->SetEventSink(&m_events);

	m_client->notifyNeedMediaData(1, 10, 42, nullptr);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyCancelNeedMediaData_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnCancelNeedMediaData(2));
	m_client->SetEventSink(&m_events);

	m_client->notifyCancelNeedMediaData(2);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyPlaybackState_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnPlaybackState(PlaybackState::SEEK_DONE));
	m_client->SetEventSink(&m_events);

	m_client->notifyPlaybackState(PlaybackState::SEEK_DONE);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyPosition_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnPosition(1'500'000'000LL));
	m_client->SetEventSink(&m_events);

	m_client->notifyPosition(1'500'000'000LL);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyDuration_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnDuration(123'456'789LL));
	m_client->SetEventSink(&m_events);

	m_client->notifyDuration(123'456'789LL);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyBufferUnderflow_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnBufferUnderflow(2));
	m_client->SetEventSink(&m_events);

	m_client->notifyBufferUnderflow(2);
}

TEST_F(AampRialtoMediaPipelineClientTest, notifyPlaybackError_ForwardsToSink)
{
	EXPECT_CALL(m_events, OnPlaybackError(3, PlaybackError::OUTPUT_PROTECTION));
	m_client->SetEventSink(&m_events);

	m_client->notifyPlaybackError(3, PlaybackError::OUTPUT_PROTECTION);
}

TEST_F(AampRialtoMediaPipelineClientTest, UnforwardedNotifications_DoNotReachSink)
{
	// StrictMock: any call fails the test.
	m_client->SetEventSink(&m_events);

	m_client->notifyNativeSize(1280, 720);
	m_client->notifyVideoData(true);
	m_client->notifyAudioData(true);
	m_client->notifyNetworkState(firebolt::rialto::NetworkState::IDLE);
	m_client->notifySourceFlushed(0);
	m_client->notifyQos(1, firebolt::rialto::QosInfo{});
	m_client->notifyFirstFrameReceived(1);
}

// ---------------------------------------------------------------------------
// Installing and removing the sink
// ---------------------------------------------------------------------------

TEST_F(AampRialtoMediaPipelineClientTest, NoSink_EventsAreDropped)
{
	EXPECT_NO_THROW(m_client->notifyNeedMediaData(1, 10, 42, nullptr));
	EXPECT_NO_THROW(m_client->notifyCancelNeedMediaData(1));
	EXPECT_NO_THROW(m_client->notifyPlaybackState(PlaybackState::PLAYING));
	EXPECT_NO_THROW(m_client->notifyPosition(0));
	EXPECT_NO_THROW(m_client->notifyDuration(0));
	EXPECT_NO_THROW(m_client->notifyBufferUnderflow(0));
	EXPECT_NO_THROW(m_client->notifyPlaybackError(0, PlaybackError::DECRYPTION));
}

TEST_F(AampRialtoMediaPipelineClientTest, SetEventSink_ReplacesExisting)
{
	StrictMock<MockPipelineEvents> second;
	EXPECT_CALL(second, OnBufferUnderflow(1));

	m_client->SetEventSink(&m_events);
	m_client->SetEventSink(&second);

	m_client->notifyBufferUnderflow(1);
}

TEST_F(AampRialtoMediaPipelineClientTest, SetEventSinkNull_StopsDelivery)
{
	m_client->SetEventSink(&m_events);
	m_client->SetEventSink(nullptr);

	m_client->notifyNeedMediaData(1, 1, 1, nullptr);
	m_client->notifyPlaybackState(PlaybackState::PLAYING);
	m_client->notifyBufferUnderflow(1);
	m_client->notifyPlaybackError(1, PlaybackError::UNKNOWN);
}

// ---------------------------------------------------------------------------
// Threading
// ---------------------------------------------------------------------------

namespace
{
/// Counts deliveries; counting is enough for the concurrency tests.
class CountingEvents : public IAampRialtoPipelineEvents
{
public:
	void OnNeedMediaData(int32_t, size_t, uint32_t) override { ++calls; }
	void OnCancelNeedMediaData(int32_t) override { ++calls; }
	void OnPlaybackState(PlaybackState) override { ++calls; }
	void OnPosition(int64_t) override { ++calls; }
	void OnDuration(int64_t) override { ++calls; }
	void OnBufferUnderflow(int32_t) override { ++calls; }
	void OnPlaybackError(int32_t, PlaybackError) override { ++calls; }

	std::atomic<int> calls{0};
};
}

TEST_F(AampRialtoMediaPipelineClientTest,
	ConcurrentSinkChangeAndDelivery_IsRaceFree)
{
	/**
	 * @brief Regression (M8): the sink is changed on the control thread while
	 *        Rialto delivers events on its own thread.  Under a thread
	 *        sanitizer an unguarded sink pointer is reported as a race.
	 */
	CountingEvents first;
	CountingEvents second;
	std::atomic<bool> done{false};

	std::thread deliverer([&] {
		while (!done.load())
		{
			m_client->notifyNeedMediaData(1, 1, 1, nullptr);
			m_client->notifyPlaybackState(PlaybackState::PLAYING);
		}
	});

	for (int i = 0; i < 2000; ++i)
	{
		m_client->SetEventSink((i % 2) ? &first : &second);
		if ((i % 7) == 0)
		{
			m_client->SetEventSink(nullptr);
		}
	}
	done = true;
	deliverer.join();

	m_client->SetEventSink(&first);
	const int before = first.calls.load();
	m_client->notifyPosition(0);
	EXPECT_EQ(first.calls.load(), before + 1);
}

TEST_F(AampRialtoMediaPipelineClientTest,
	Sink_MayReplaceItselfWhileRunning_DoesNotDeadlock)
{
	/**
	 * @brief The sink is called without the client's lock, so a sink that
	 *        installs another sink (or clears it) must not deadlock.
	 */
	CountingEvents second;
	EXPECT_CALL(m_events, OnNeedMediaData(1, 1u, 1u))
		.WillOnce([&](int32_t, size_t, uint32_t) {
			m_client->SetEventSink(&second);
		});
	m_client->SetEventSink(&m_events);

	auto result = std::async(std::launch::async, [&] {
		m_client->notifyNeedMediaData(1, 1, 1, nullptr);
		m_client->notifyNeedMediaData(1, 1, 2, nullptr);
	});
	ASSERT_EQ(result.wait_for(std::chrono::seconds(5)),
		std::future_status::ready);
	EXPECT_EQ(second.calls.load(), 1);
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
