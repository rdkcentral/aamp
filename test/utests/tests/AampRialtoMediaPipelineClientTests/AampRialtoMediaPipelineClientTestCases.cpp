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
