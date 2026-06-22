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
 * @file PlayerDirectRialtoCCManagerTestCases.cpp
 * @brief L1 unit tests for PlayerDirectRialtoCCManager.
 *
 * Tests verify the CC manager's orchestration of its IDirectRialtoCC
 * dependency using a strict mock.  Per the L1 golden rule, every assertion
 * verifies how the component under test (PlayerDirectRialtoCCManager)
 * behaves, not how the mock behaves.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>

#include "PlayerDirectRialtoCCManager.h"
#include "IDirectRialtoCC.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

class MockIDirectRialtoCC : public IDirectRialtoCC
{
public:
	MOCK_METHOD(bool, setTextTrackIdentifier, (const std::string &id), (override));
	MOCK_METHOD(bool, setCCMute, (bool muted), (override));
};

class PlayerDirectRialtoCCManagerTestable : public PlayerDirectRialtoCCManager
{
public:
	using PlayerDirectRialtoCCManager::Initialize;
	using PlayerDirectRialtoCCManager::StartRendering;
	using PlayerDirectRialtoCCManager::StopRendering;
	using PlayerDirectRialtoCCManager::ResetState;
};

class PlayerDirectRialtoCCManagerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_mock = std::make_unique<StrictMock<MockIDirectRialtoCC>>();
	}

	void TearDown() override
	{
		m_mock.reset();
	}

	void InitWithDefaultTrack()
	{
		EXPECT_CALL(*m_mock, setTextTrackIdentifier("CC1"))
			.Times(1)
			.WillOnce(Return(true));
		m_mgr.Initialize(m_mock.get());
	}

	PlayerDirectRialtoCCManagerTestable    m_mgr;
	std::unique_ptr<MockIDirectRialtoCC>   m_mock;
};

/**
 * @test Initialize_WithNullHandle_DoesNotCallControl
 * @brief Passing nullptr must not crash and must not call the mock.
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	Initialize_WithNullHandle_DoesNotCallControl)
{
	EXPECT_NO_FATAL_FAILURE(m_mgr.Initialize(nullptr));
}

/**
 * @test Initialize_WithHandle_SetsDefaultTrackCC1
 * @brief When no track is cached, Initialize() should apply "CC1" as the
 *        default text-track identifier.
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	Initialize_WithHandle_SetsDefaultTrackCC1)
{
	EXPECT_CALL(*m_mock, setTextTrackIdentifier("CC1"))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.Initialize(m_mock.get());
}

/**
 * @test Initialize_WithCachedTrack_ReappliesCachedTrack
 * @brief When a track has already been cached via SetTrack(), Initialize()
 *        with a new handle must re-apply the cached identifier, not "CC1".
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	Initialize_WithCachedTrack_ReappliesCachedTrack)
{
	m_mgr.SetTrack("CC3");

	EXPECT_CALL(*m_mock, setTextTrackIdentifier("CC3"))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.Initialize(m_mock.get());
}

/**
 * @test SetTrack_NumericWith608Format_PrependsCCPrefix
 * @brief A numeric track string with 608 format should become "CC<n>".
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	SetTrack_NumericWith608Format_PrependsCCPrefix)
{
	InitWithDefaultTrack();

	EXPECT_CALL(*m_mock, setTextTrackIdentifier("CC2"))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.SetTrack("2", eCLOSEDCAPTION_FORMAT_608);
}

/**
 * @test SetTrack_NumericWith708Format_PrependsServicePrefix
 * @brief A numeric track string with 708 format should become "SERVICE<n>".
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	SetTrack_NumericWith708Format_PrependsServicePrefix)
{
	InitWithDefaultTrack();

	EXPECT_CALL(*m_mock, setTextTrackIdentifier("SERVICE3"))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.SetTrack("3", eCLOSEDCAPTION_FORMAT_708);
}

/**
 * @test SetTrack_AlphabeticTrack_PassedThrough
 * @brief A track string that already has an alphabetic prefix must be
 *        forwarded unchanged.
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	SetTrack_AlphabeticTrack_PassedThrough)
{
	InitWithDefaultTrack();

	EXPECT_CALL(*m_mock, setTextTrackIdentifier("CC1"))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.SetTrack("CC1");
}

/**
 * @test StartRendering_CallsCCMuteFalse
 * @brief StartRendering() must un-mute CC by calling setCCMute(false).
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	StartRendering_CallsCCMuteFalse)
{
	InitWithDefaultTrack();

	EXPECT_CALL(*m_mock, setCCMute(false))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.StartRendering();
}

/**
 * @test StopRendering_CallsCCMuteTrue
 * @brief StopRendering() must mute CC by calling setCCMute(true).
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	StopRendering_CallsCCMuteTrue)
{
	InitWithDefaultTrack();

	EXPECT_CALL(*m_mock, setCCMute(true))
		.Times(1)
		.WillOnce(Return(true));

	m_mgr.StopRendering();
}

/**
 * @test ResetState_ClearsControlHandle
 * @brief After ResetState(), StartRendering() must not call the old mock.
 */
TEST_F(PlayerDirectRialtoCCManagerTest,
	ResetState_ClearsControlHandle)
{
	InitWithDefaultTrack();

	m_mgr.ResetState();

	EXPECT_NO_FATAL_FAILURE(m_mgr.StartRendering());
}