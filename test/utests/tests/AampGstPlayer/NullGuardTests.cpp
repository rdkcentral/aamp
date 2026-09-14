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

/**
 * @file NullGuardTests.cpp
 * @brief L1 unit tests for null-pointer guards added in VPAAMP-309.
 *
 * Each test constructs an AAMPGstPlayer, manipulates one pointer to null,
 * invokes the guarded code path, and asserts that no crash occurs and no
 * downstream side-effects are triggered.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "InterfacePlayerRDK.h"
#include "aampgstplayer.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampUtils.h"

using ::testing::An;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

// gpGlobalConfig is defined once in FunctionalTests.cpp (same link unit).
extern AampConfig *gpGlobalConfig;

/**
 * @brief Fixture for VPAAMP-309 null-pointer guard tests.
 *
 * Provides a minimally constructed AAMPGstPlayer (no pipeline) together with
 * the mock infrastructure required by aampgstplayer.cpp and its dependencies.
 * NiceMock is used for GStreamer / GLib so that incidental framework calls
 * made during construction and destruction do not cause unexpected-call
 * failures.
 */
class AAMPGstPlayerNullGuardTests : public ::testing::Test
{
protected:
	AAMPGstPlayer      *mPlayer{nullptr};
	PrivateInstanceAAMP *mAamp{nullptr};

	void SetUp() override
	{
		g_mockAampUtils            = std::make_shared<NiceMock<MockAampUtils>>();
		g_mockGStreamer             = new NiceMock<MockGStreamer>();
		g_mockGLib                 = std::make_shared<NiceMock<MockGLib>>();
		g_mockAampConfig           = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockPrivateInstanceAAMP  = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		mAamp                      = new PrivateInstanceAAMP{};
	}

	void TearDown() override
	{
		// Delete mPlayer first so its destructor runs while all mocks are
		// still live.  This also handles the case where a test exits early
		// via ASSERT_* without calling DestroyPlayer().
		if (mPlayer != nullptr)
		{
			// Restore aamp so the destructor does not access a dangling pointer.
			if (mPlayer->aamp == nullptr)
			{
				mPlayer->aamp = mAamp;
			}
			delete mPlayer;
			mPlayer = nullptr;
		}

		g_mockPrivateInstanceAAMP.reset();

		g_mockAampConfig.reset();

		g_mockGLib.reset();

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		g_mockAampUtils.reset();

		delete mAamp;
		mAamp = nullptr;
	}

	/**
	 * @brief Construct a minimal AAMPGstPlayer (no pipeline configured).
	 *
	 * Mirrors ConstructAMPGstPlayer() in FunctionalTests.cpp.  The debug-level
	 * config call is expected exactly once because the test string is non-empty.
	 */
	void ConstructPlayer()
	{
		const std::string debugLevel{"test_level"};
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
			.WillOnce(Return(debugLevel));

		mPlayer = new AAMPGstPlayer{mAamp, nullptr};
	}

	void DestroyPlayer()
	{
		delete mPlayer;
		mPlayer = nullptr;
	}
};

// ---------------------------------------------------------------------------
// SetEncryptedAamp – null aamp parameter
// ---------------------------------------------------------------------------

/**
 * @test Passing null to SetEncryptedAamp must not dereference the pointer.
 *       The encrypted-aamp id should be set to -1 (sentinel) and the
 *       encrypted-aamp pointer to null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, SetEncryptedAamp_NullAamp_NoSegfault)
{
	ConstructPlayer();

	// Must not crash; no GStreamer setEncryption call expected.
	EXPECT_CALL(*g_mockGStreamer, gst_debug_set_threshold_from_string(_, _)).Times(0);
	mPlayer->SetEncryptedAamp(nullptr);

	EXPECT_EQ(mPlayer->mEncryptedAamp,    nullptr);
	EXPECT_EQ(mPlayer->mEncryptedAampId, -1);

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// SetEncryptedAamp – aamp with null mDRMLicenseManager
// ---------------------------------------------------------------------------

/**
 * @test When aamp is non-null but mDRMLicenseManager is null, SetEncryptedAamp
 *       must not dereference mDRMLicenseManager.
 *       The default PrivateInstanceAAMP constructor leaves mDRMLicenseManager
 *       null (initialised to NULL in priv_aamp.cpp), so mAamp is suitable.
 */
TEST_F(AAMPGstPlayerNullGuardTests, SetEncryptedAamp_NullDrmLicenseManager_NoSegfault)
{
	ConstructPlayer();

	// mAamp->mDRMLicenseManager is null by default – guard must fire cleanly.
	mPlayer->SetEncryptedAamp(mAamp);

	// mEncryptedAamp should be updated even when the DRM manager is absent.
	EXPECT_EQ(mPlayer->mEncryptedAamp, mAamp);

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// ChangeAamp – null newAamp
// ---------------------------------------------------------------------------

/**
 * @test Passing null to ChangeAamp must not dereference the pointer.
 *       The function should update the internal aamp pointer and the id3
 *       handler, then return without touching playerInstance.
 */
TEST_F(AAMPGstPlayerNullGuardTests, ChangeAamp_NullAamp_NoSegfault)
{
	ConstructPlayer();

	// Must not crash; no ResumeInjector or DisableDecoderHandleNotified call expected.
	mPlayer->ChangeAamp(nullptr, nullptr);

	EXPECT_EQ(mPlayer->aamp, nullptr);

	// Restore a valid aamp pointer so the destructor path is safe.
	mPlayer->aamp = mAamp;

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// StartMonitorAvTimer – null aamp
// ---------------------------------------------------------------------------

/**
 * @test StartMonitorAvTimer must return immediately when aamp is null, without
 *       calling g_timeout_add.
 */
TEST_F(AAMPGstPlayerNullGuardTests, StartMonitorAvTimer_NullAamp_NoTimerAdded)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	// Guard must prevent any timer from being scheduled.
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)).Times(0);
	mPlayer->StartMonitorAvTimer();

	// Restore aamp for safe destruction.
	mPlayer->aamp = mAamp;

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// StartMonitorAvTimer – monitorAVReportingInterval is read at start time
// ---------------------------------------------------------------------------

/**
 * @test A monitorAVReportingInterval applied after the sink was constructed must
 *       be honoured.
 *
 * AAMPGstPlayer is created before the app applies its configuration, so the
 * interval cached by the constructor is stale by the time the timer starts.
 * StartMonitorAvTimer() must therefore re-read the config rather than use that
 * snapshot, otherwise an app requesting sub-second AV monitoring silently gets
 * the 1000 ms default.
 */
TEST_F(AAMPGstPlayerNullGuardTests, StartMonitorAvTimer_UsesIntervalAppliedAfterConstruction)
{
	/* Construct the sink first, with no config attached, mirroring production
	 * ordering: the sink exists before the app calls initConfig(). */
	ConstructPlayer();

	constexpr int kRequestedIntervalMs = 100;
	AampConfig config;
	mAamp->mConfig = &config;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_MonitorAV))
		.WillRepeatedly(Return(true));
	/* Interval requested by the app after construction. */
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(An<AAMPConfigSettingInt>()))
		.WillRepeatedly(Return(kRequestedIntervalMs));

	EXPECT_CALL(*g_mockGLib, g_timeout_add(kRequestedIntervalMs, _, _))
		.WillOnce(Return(1));

	mPlayer->StartMonitorAvTimer();

	/* The cached member must reflect the applied value so that the timeInState
	 * clamp in MonitorAvTimerCallback() uses the same interval. */
	EXPECT_EQ(mPlayer->GetMonitorAVInterval(), kRequestedIntervalMs);

	mPlayer->StopMonitorAvTimer();
	mAamp->mConfig = nullptr;

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// StartMonitorAvTimer – default interval is unchanged
// ---------------------------------------------------------------------------

/**
 * @test With no override applied, StartMonitorAvTimer() must still schedule at
 *       the default reporting interval.
 */
TEST_F(AAMPGstPlayerNullGuardTests, StartMonitorAvTimer_DefaultIntervalUnchanged)
{
	ConstructPlayer();

	AampConfig config;
	mAamp->mConfig = &config;

	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_MonitorAV))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(An<AAMPConfigSettingInt>()))
		.WillRepeatedly(Return(DEFAULT_MONITOR_AV_REPORTING_INTERVAL));

	EXPECT_CALL(*g_mockGLib, g_timeout_add(DEFAULT_MONITOR_AV_REPORTING_INTERVAL, _, _))
		.WillOnce(Return(1));

	mPlayer->StartMonitorAvTimer();

	EXPECT_EQ(mPlayer->GetMonitorAVInterval(), DEFAULT_MONITOR_AV_REPORTING_INTERVAL);

	mPlayer->StopMonitorAvTimer();
	mAamp->mConfig = nullptr;

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: HandleOnGstDecodeErrorCb – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The decode-error callback must not dereference _this->aamp when it is
 *       null.  The callback is invoked via the registered lambda stored in
 *       playerInstance->OnGstDecodeErrorCb.
 */
TEST_F(AAMPGstPlayerNullGuardTests, HandleOnGstDecodeErrorCb_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	// Invoke the registered callback; the guard must catch the null and return.
	ASSERT_TRUE(mPlayer->playerInstance->OnGstDecodeErrorCb);
	mPlayer->playerInstance->OnGstDecodeErrorCb(1);

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: HandleOnGstPtsErrorCb – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The PTS-error callback must not dereference _this->aamp when null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, HandleOnGstPtsErrorCb_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->OnGstPtsErrorCb);
	mPlayer->playerInstance->OnGstPtsErrorCb(true /*isVideo*/, false /*isAudioSink*/);

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: HandleOnGstBufferUnderflowCb – null privateContext
// ---------------------------------------------------------------------------

/**
 * @test The underflow callback must not dereference _this->privateContext when
 *       it is null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, HandleOnGstBufferUnderflowCb_NullPrivateContext_NoSegfault)
{
	ConstructPlayer();

	// Detach privateContext – guard must fire before any buffer-control access.
	AAMPGstPlayerPriv *savedCtx = mPlayer->privateContext;
	mPlayer->privateContext     = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->OnGstBufferUnderflowCb);
	mPlayer->playerInstance->OnGstBufferUnderflowCb(static_cast<int>(eMEDIATYPE_VIDEO));

	// Restore so the destructor can SAFE_DELETE it.
	mPlayer->privateContext = savedCtx;

	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: HandleOnGstBufferUnderflowCb – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The underflow callback must not dereference _this->aamp (or mConfig)
 *       when aamp is null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, HandleOnGstBufferUnderflowCb_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->OnGstBufferUnderflowCb);
	mPlayer->playerInstance->OnGstBufferUnderflowCb(static_cast<int>(eMEDIATYPE_VIDEO));

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: NeedDataCb – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The need-data callback must not dereference _this->aamp when null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, NeedData_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->NeedDataCb);
	mPlayer->playerInstance->NeedDataCb(static_cast<int>(eMEDIATYPE_VIDEO));

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: NeedDataCb – null privateContext
// ---------------------------------------------------------------------------

/**
 * @test The need-data callback must not dereference _this->privateContext when
 *       it is null.  Both aamp and privateContext are checked by the guard, so
 *       we verify the privateContext branch independently.
 */
TEST_F(AAMPGstPlayerNullGuardTests, NeedData_NullPrivateContext_NoSegfault)
{
	ConstructPlayer();

	AAMPGstPlayerPriv *savedCtx = mPlayer->privateContext;
	mPlayer->privateContext     = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->NeedDataCb);
	mPlayer->playerInstance->NeedDataCb(static_cast<int>(eMEDIATYPE_VIDEO));

	mPlayer->privateContext = savedCtx;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: EnoughDataCb – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The enough-data callback must not dereference _this->aamp when null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, EnoughData_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->EnoughDataCb);
	mPlayer->playerInstance->EnoughDataCb(static_cast<int>(eMEDIATYPE_VIDEO));

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: EnoughDataCb – null privateContext
// ---------------------------------------------------------------------------

/**
 * @test The enough-data callback must not dereference _this->privateContext
 *       when it is null.
 */
TEST_F(AAMPGstPlayerNullGuardTests, EnoughData_NullPrivateContext_NoSegfault)
{
	ConstructPlayer();

	AAMPGstPlayerPriv *savedCtx = mPlayer->privateContext;
	mPlayer->privateContext     = nullptr;

	ASSERT_TRUE(mPlayer->playerInstance->EnoughDataCb);
	mPlayer->playerInstance->EnoughDataCb(static_cast<int>(eMEDIATYPE_VIDEO));

	mPlayer->privateContext = savedCtx;
	DestroyPlayer();
}

// ---------------------------------------------------------------------------
// Callback: HandleBusMessage – null aamp
// ---------------------------------------------------------------------------

/**
 * @test The bus-message callback must not dereference _this->aamp when null.
 *       A MESSAGE_ERROR event is used as it is the first case handled in the
 *       switch, maximising the guard's coverage opportunity.
 */
TEST_F(AAMPGstPlayerNullGuardTests, HandleBusMessage_NullAamp_NoSegfault)
{
	ConstructPlayer();

	mPlayer->aamp = nullptr;

	BusEventData event{};
	event.msgType = MESSAGE_ERROR;
	event.msg     = "test error";

	ASSERT_TRUE(mPlayer->playerInstance->busMessageCallback);
	mPlayer->playerInstance->busMessageCallback(event);

	mPlayer->aamp = mAamp;
	DestroyPlayer();
}
