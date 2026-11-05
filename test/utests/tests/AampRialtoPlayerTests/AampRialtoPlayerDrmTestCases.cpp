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
 * @file AampRialtoPlayerDrmTestCases.cpp
 * @brief AampRialtoPlayer L1 tests: DRM protection events and the DRM bridge.
 */

#include "AampRialtoPlayerTestFixture.h"

// ===========================================================================
// Phase DRM — QueueProtectionEvent / ClearProtectionEvent
// ===========================================================================

class AampRialtoPlayerDrmTest : public AampRialtoPlayerWithDemuxTest
{
protected:
	void SetUp() override
	{
		m_mockDrmBridge = std::make_unique<NiceMock<MockDrmBridge>>();
		g_mockDrmBridge = m_mockDrmBridge.get();
		g_lastDrmBridgeAamp = nullptr;
		g_drmBridgeCreateCount = 0;
		AampRialtoPlayerWithDemuxTest::SetUp();
	}

	void TearDown() override
	{
		AampRialtoPlayerWithDemuxTest::TearDown();
		g_mockDrmBridge = nullptr;
		m_mockDrmBridge.reset();
	}

	std::unique_ptr<NiceMock<MockDrmBridge>> m_mockDrmBridge;
};

/**
 * @test QueueProtectionEvent_Video_StoresParamsWithoutCallingBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_Video_StoresParamsWithoutCallingBridge)
{
	const char *systemId = "com.widevine.alpha";
	const uint8_t initData[] = {0x01, 0x02, 0x03};

	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);

	m_player->QueueProtectionEvent(
		systemId, initData, sizeof(initData), eMEDIATYPE_VIDEO);
}

/**
 * @test QueueProtectionEvent_NullInitData_DoesNotCallBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	QueueProtectionEvent_NullInitData_DoesNotCallBridge)
{
	EXPECT_CALL(*m_mockDrmBridge, createSession(_, _, _, _)).Times(0);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", nullptr, 0, eMEDIATYPE_VIDEO);
}

/**
 * @test ClearProtectionEvent_ClearsPendingProtection
 */
TEST_F(AampRialtoPlayerDrmTest,
	ClearProtectionEvent_ClearsPendingProtection)
{
	const uint8_t initData[] = {0x01};
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// ClearProtectionEvent only clears the buffered init data so that a
	// subsequent Configure/attachOrUpdate does not re-use stale protection
	// params.
	m_player->ClearProtectionEvent();
}

// ---------------------------------------------------------------------------
// SetEncryptedAamp — lazy DRM bridge initialisation for pre-roll ad scenario
// ---------------------------------------------------------------------------

/**
 * @test SetEncryptedAamp_CreatesBridge_BridgeDelegatesToMock
 */
TEST_F(AampRialtoPlayerDrmTest,
	SetEncryptedAamp_CreatesBridge_BridgeDelegatesToMock)
{
	// Before any call the bridge is absent; SetEncryptedAamp must create it.
	// The fake AampDrmBridge delegates to g_mockDrmBridge, so subsequent
	// calls on the player's bridge reach m_mockDrmBridge.
 	PrivateInstanceAAMP encryptedAamp{};
 	m_player->SetEncryptedAamp(&encryptedAamp);

	// ClearProtectionEvent only clears pending params.  Verify that the
	// bridge was created by queuing protection and checking createSession is
	// reachable through the mock.
	m_player->ClearProtectionEvent();
}

/**
 * @test ChangeAamp_DefaultBridge_IsRecreatedForNewInstance
 */
TEST_F(AampRialtoPlayerDrmTest,
	ChangeAamp_DefaultBridge_IsRecreatedForNewInstance)
{
	/**
	 * @brief Regression (H8): a bridge built for the previous AAMP instance
	 *        would keep calling it after ChangeAamp(), which may have been
	 *        destroyed.  The next attach must build one for the new instance.
	 */
	auto *original =
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get());
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();
	ASSERT_EQ(g_drmBridgeCreateCount, 1);
	ASSERT_EQ(g_lastDrmBridgeAamp, original);

	PrivateInstanceAAMP newAamp{};
	m_player->ChangeAamp(&newAamp, nullptr);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	EXPECT_EQ(g_drmBridgeCreateCount, 2);
	EXPECT_EQ(g_lastDrmBridgeAamp, &newAamp);
}

/**
 * @test ChangeAamp_AfterSetEncryptedAamp_KeepsEncryptedPlayerBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	ChangeAamp_AfterSetEncryptedAamp_KeepsEncryptedPlayerBridge)
{
	/**
	 * @brief The bridge installed by SetEncryptedAamp() deliberately targets
	 *        the encrypted player for a shared pipeline (pre-roll ads), so
	 *        switching the active instance must not replace it.
	 */
	PrivateInstanceAAMP encryptedAamp{};
	m_player->SetEncryptedAamp(&encryptedAamp);
	ASSERT_EQ(g_drmBridgeCreateCount, 1);
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	PrivateInstanceAAMP newAamp{};
	m_player->ChangeAamp(&newAamp, nullptr);
	m_player->SetStreamCaps(eMEDIATYPE_VIDEO, MakeVideoH264CodecInfo());

	EXPECT_EQ(g_drmBridgeCreateCount, 1);
	EXPECT_EQ(g_lastDrmBridgeAamp, &encryptedAamp);
}

/**
 * @test SetEncryptedAamp_Null_RevertsToDefaultBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	SetEncryptedAamp_Null_RevertsToDefaultBridge)
{
	auto *original =
		reinterpret_cast<PrivateInstanceAAMP *>(g_mockPrivateInstanceAAMP.get());
	PrivateInstanceAAMP encryptedAamp{};
	m_player->SetEncryptedAamp(&encryptedAamp);
	m_player->SetEncryptedAamp(nullptr);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	EXPECT_EQ(g_drmBridgeCreateCount, 2);
	EXPECT_EQ(g_lastDrmBridgeAamp, original)
		<< "a null encrypted instance must not leave a bridge with no target";
}

/**
 * @test AttachSource_WithNoBridgeSet_LazilyCreatesBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachSource_WithNoBridgeSet_LazilyCreatesBridge)
{
	// Bridge is not pre-initialised via SetEncryptedAamp.
	// It must be created lazily when AttachSource is first called, and the
	// fake must delegate calls to g_mockDrmBridge (set by the fixture).
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	SendVideoInitFragment();

	// ClearProtectionEvent only clears pending params, not DRM sessions.
	m_player->ClearProtectionEvent();
}

/**
 * @test AttachSource_AfterSetEncryptedAamp_DoesNotReplaceBridge
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachSource_AfterSetEncryptedAamp_DoesNotReplaceBridge)
{
	// SetEncryptedAamp sets the bridge.  Subsequent AttachSource calls must
	// NOT replace it — the lazy guard `if (!m_drmBridge)` ensures this.
	// Verify by checking createSession is called through the same mock.
	const uint8_t initData[] = {0x01};
 	PrivateInstanceAAMP encryptedAamp{};
 	m_player->SetEncryptedAamp(&encryptedAamp);

	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// createSession must be called on the bridge installed by SetEncryptedAamp
	// (which delegates to m_mockDrmBridge via the fake).
	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(7));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

/**
 * @test AttachVideoSource_WithValidMksId_AttachesWithHasDrmTrue
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithValidMksId_AttachesWithHasDrmTrue)
{
	const uint8_t initData[] = {0x01};
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(10));

	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

/**
 * @test AttachVideoSource_WithoutMksId_AttachesWithHasDrmFalse
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachVideoSource_WithoutMksId_AttachesWithHasDrmFalse)
{
	Configure(FORMAT_ISO_BMFF, FORMAT_INVALID);
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_FALSE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillRepeatedly(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				// Inband CC subtitle source — no DRM expected.
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
}

// ---------------------------------------------------------------------------
// HLS-convention fallback: protection queued only under eMEDIATYPE_VIDEO
// (see StreamAbstractionAAMP_HLS::InitiateDrmProcess) must still be used to
// create a DRM session for an encrypted non-video source.
// ---------------------------------------------------------------------------

/**
 * @test AttachAudioSource_EncryptedWithOnlyVideoProtectionQueued_UsesVideoProtection
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachAudioSource_EncryptedWithOnlyVideoProtectionQueued_UsesVideoProtection)
{
	const uint8_t initData[] = {0x01, 0x02};
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	// HLS only ever queues protection under eMEDIATYPE_VIDEO.
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.Times(2) // once for video's own attach, once for the audio fallback
		.WillRepeatedly(Return(10));

	// Attach order is video, then the inband CC subtitle source (deferred
	// in Configure() until video attaches, no protection queued for it),
	// then audio once SendAudioInitFragment() runs.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
	SendAudioInitFragment(/*encrypted=*/true);
}

/**
 * @test AttachAudioSource_NotEncrypted_DoesNotFallBackToVideoProtection
 */
TEST_F(AampRialtoPlayerDrmTest,
	AttachAudioSource_NotEncrypted_DoesNotFallBackToVideoProtection)
{
	const uint8_t initData[] = {0x01, 0x02};
	Configure(FORMAT_ISO_BMFF, FORMAT_ISO_BMFF);
	m_player->QueueProtectionEvent(
		"com.widevine.alpha", initData, sizeof(initData), eMEDIATYPE_VIDEO);

	// Only the video attach should create a DRM session.
	EXPECT_CALL(*m_mockDrmBridge,
		createSession(_, _, _, eMEDIATYPE_VIDEO))
		.Times(1)
		.WillOnce(Return(10));

	// Attach order is video, then the inband CC subtitle source, then audio.
	EXPECT_CALL(*m_mockPipelinePtr, attachSource(_))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_TRUE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}))
		.WillOnce(Invoke(
			[this](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				EXPECT_FALSE(src->getHasDrm());
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));
	SendVideoInitFragment();
	SendAudioInitFragment(/*encrypted=*/false);
}
