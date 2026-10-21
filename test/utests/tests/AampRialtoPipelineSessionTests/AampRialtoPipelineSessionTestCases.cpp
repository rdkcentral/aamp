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
 * @file AampRialtoPipelineSessionTestCases.cpp
 * @brief L1 tests for AampRialtoPipelineSession.
 *
 * Oracle: the session's documented contract.  CreateAndLoad() reports why it
 * failed, keeps a created pipeline even when load() fails, installs the event
 * sink before load() (events sent while loading are not lost), and the
 * destructor releases the pipeline before clearing the sink, so an event
 * delivered while the pipeline is being released still reaches the player.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <functional>
#include <memory>

#include "AampRialtoPipelineSession.h"
#include "MockIMediaPipeline.h"
#include "MockIMediaPipelineCapabilities.h"
#include "MockIMediaPipelineFactory.h"
#include "MockIRialtoControlBackend.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using firebolt::rialto::IMediaPipeline;
using firebolt::rialto::IMediaPipelineClient;
using firebolt::rialto::PlaybackError;
using firebolt::rialto::PlaybackState;
using CreateResult = AampRialtoPipelineSession::CreateResult;

namespace
{
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

/// A pipeline that runs @c onDestroy from its destructor.
class TrackedPipeline : public NiceMock<MockIMediaPipeline>
{
public:
	~TrackedPipeline() override
	{
		if (onDestroy)
		{
			onDestroy();
		}
	}
	std::function<void()> onDestroy;
};
} // namespace

class AampRialtoPipelineSessionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_factory = std::make_shared<NiceMock<MockIMediaPipelineFactory>>();
		m_pipeline = std::make_unique<TrackedPipeline>();
		m_pipelinePtr = m_pipeline.get();
		ON_CALL(*m_pipelinePtr, load(_, _, _, _)).WillByDefault(Return(true));
		ON_CALL(*m_factory, createMediaPipeline(_, _))
			.WillByDefault(Invoke(
				[this](std::weak_ptr<IMediaPipelineClient> client,
				       const firebolt::rialto::VideoRequirements &)
					-> std::unique_ptr<IMediaPipeline>
				{
					m_client = client;
					return std::move(m_pipeline);
				}));
		g_mockPipelineFactory = m_factory;
		g_mockCapabilitiesFactory =
			std::make_shared<NiceMock<MockIMediaPipelineCapabilitiesFactory>>();

		auto backend = std::make_unique<NiceMock<MockIRialtoControlBackend>>();
		m_backend = backend.get();
		ON_CALL(*m_backend, waitForRunning(_)).WillByDefault(Return(true));
		m_session = std::make_unique<AampRialtoPipelineSession>(std::move(backend));
	}

	void TearDown() override
	{
		m_session.reset();
		g_mockPipelineFactory.reset();
		g_mockCapabilitiesFactory.reset();
	}

	/// Deliver a position event the way Rialto does, through the client.
	void DeliverPosition(int64_t positionNs)
	{
		if (auto client = m_client.lock())
		{
			client->notifyPosition(positionNs);
		}
	}

	std::shared_ptr<NiceMock<MockIMediaPipelineFactory>> m_factory;
	std::unique_ptr<TrackedPipeline> m_pipeline;
	TrackedPipeline *m_pipelinePtr{nullptr};
	NiceMock<MockIRialtoControlBackend> *m_backend{nullptr};
	std::weak_ptr<IMediaPipelineClient> m_client;
	NiceMock<MockPipelineEvents> m_events;
	std::unique_ptr<AampRialtoPipelineSession> m_session;
};

// ---------------------------------------------------------------------------
// CreateAndLoad results
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_Success_LoadsMse
 * @brief The pipeline is created, loaded as MSE and returned by GetPipeline().
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_Success_LoadsMse)
{
	EXPECT_CALL(*m_pipelinePtr,
		load(firebolt::rialto::MediaType::MSE, _, _, _)).WillOnce(Return(true));

	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
	EXPECT_EQ(m_session->GetPipeline().get(), m_pipelinePtr);
}

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_NoFactory_ReturnsFactoryUnavailable
 * @brief Without a Rialto server there is no factory; nothing is created.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_NoFactory_ReturnsFactoryUnavailable)
{
	g_mockPipelineFactory.reset();

	EXPECT_EQ(m_session->CreateAndLoad(m_events),
		CreateResult::FactoryUnavailable);
	EXPECT_EQ(m_session->GetPipeline(), nullptr);
}

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_CreateFails_ReturnsCreateFailed
 * @brief createMediaPipeline() returning null leaves no pipeline.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_CreateFails_ReturnsCreateFailed)
{
	EXPECT_CALL(*m_factory, createMediaPipeline(_, _))
		.WillOnce(Invoke([](auto, const auto &) {
			return std::unique_ptr<IMediaPipeline>{};
		}));

	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::CreateFailed);
	EXPECT_EQ(m_session->GetPipeline(), nullptr);
}

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_LoadFails_KeepsPipeline
 * @brief A failed load() is reported, but the pipeline is kept so the player
 *        can still stop and release it.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_LoadFails_KeepsPipeline)
{
	EXPECT_CALL(*m_pipelinePtr, load(_, _, _, _)).WillOnce(Return(false));

	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::LoadFailed);
	EXPECT_EQ(m_session->GetPipeline().get(), m_pipelinePtr);
}

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_WaitsForRunningBeforeCreating
 * @brief The pipeline is created only after the RUNNING wait, and still
 *        created when that wait times out.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_WaitsForRunningBeforeCreating)
{
	{
		InSequence order;
		EXPECT_CALL(*m_backend, waitForRunning(_)).WillOnce(Return(false));
		EXPECT_CALL(*m_factory, createMediaPipeline(_, _));
	}

	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
}

// ---------------------------------------------------------------------------
// Event sink
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoPipelineSession_CreateAndLoad_EventDuringLoad_ReachesSink
 * @brief Rialto may send events while load() runs; the sink is installed
 *        before load() so they are delivered.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_CreateAndLoad_EventDuringLoad_ReachesSink)
{
	EXPECT_CALL(*m_pipelinePtr, load(_, _, _, _))
		.WillOnce(Invoke([this](auto, const auto &, const auto &, bool) {
			DeliverPosition(5);
			return true;
		}));
	EXPECT_CALL(m_events, OnPosition(5));

	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
}

/**
 * @test AampRialtoPipelineSession_ClearClientEventSink_StopsDelivery
 * @brief After the sink is cleared, events go nowhere.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_ClearClientEventSink_StopsDelivery)
{
	ASSERT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
	EXPECT_CALL(m_events, OnPosition(_)).Times(0);

	m_session->ClearClientEventSink();
	DeliverPosition(6);
}

/**
 * @test AampRialtoPipelineSession_Destructor_ReleasesPipelineBeforeClearingSink
 * @brief An event delivered while the pipeline is being released (Rialto
 *        drains its event thread then) still reaches the sink.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_Destructor_ReleasesPipelineBeforeClearingSink)
{
	ASSERT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
	bool destroyed = false;
	m_pipelinePtr->onDestroy = [this, &destroyed] {
		destroyed = true;
		DeliverPosition(7);
	};
	EXPECT_CALL(m_events, OnPosition(7));

	m_session.reset();
	EXPECT_TRUE(destroyed);
}

// ---------------------------------------------------------------------------
// Capabilities and pipeline ownership
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoPipelineSession_Capabilities_CreatedOnFirstCreateAndLoad
 * @brief Capabilities are null until CreateAndLoad() and then created once.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_Capabilities_CreatedOnFirstCreateAndLoad)
{
	auto capabilitiesFactory =
		std::make_shared<NiceMock<MockIMediaPipelineCapabilitiesFactory>>();
	g_mockCapabilitiesFactory = capabilitiesFactory;
	EXPECT_CALL(*capabilitiesFactory, createMediaPipelineCapabilities())
		.WillOnce(Invoke([] {
			return std::make_unique<NiceMock<MockIMediaPipelineCapabilities>>();
		}));
	EXPECT_EQ(m_session->Capabilities(), nullptr);

	ASSERT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
	EXPECT_NE(m_session->Capabilities(), nullptr);

	// A second session on the same object reuses them.
	m_pipeline = std::make_unique<TrackedPipeline>();
	ON_CALL(*m_pipeline, load(_, _, _, _)).WillByDefault(Return(true));
	EXPECT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
}

/**
 * @test AampRialtoPipelineSession_SetPipeline_SnapshotOutlivesRelease
 * @brief A GetPipeline() snapshot stays valid after the session releases its
 *        own reference; the pipeline goes when the last snapshot does.
 */
TEST_F(AampRialtoPipelineSessionTest,
	AampRialtoPipelineSession_SetPipeline_SnapshotOutlivesRelease)
{
	ASSERT_EQ(m_session->CreateAndLoad(m_events), CreateResult::Loaded);
	bool destroyed = false;
	m_pipelinePtr->onDestroy = [&destroyed] { destroyed = true; };

	auto snapshot = m_session->GetPipeline();
	m_session->SetPipeline(nullptr);
	EXPECT_EQ(m_session->GetPipeline(), nullptr);
	EXPECT_FALSE(destroyed);

	snapshot.reset();
	EXPECT_TRUE(destroyed);
}
