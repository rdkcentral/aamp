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
 * @file BusMessageErrorTests.cpp
 * @brief L1 tests for the MESSAGE_ERROR handling in HandleBusMessage().
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
#include "MockAampLicManager.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

class AAMPGstPlayerBusErrorTests : public ::testing::Test
{
protected:
	AAMPGstPlayer *mPlayer{nullptr};
	PrivateInstanceAAMP *mAamp{nullptr};
	AampConfig *mConfig{nullptr};
	AampDRMLicenseManager *mLicenseManager{nullptr};

	void SetUp() override
	{
		g_mockAampUtils = std::make_shared<NiceMock<MockAampUtils>>();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = std::make_shared<NiceMock<MockGLib>>();
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		g_mockAampLicenseManager = std::make_shared<NiceMock<MockAampLicenseManager>>();

		mConfig = new AampConfig();
		mAamp = new PrivateInstanceAAMP{mConfig};
		mLicenseManager = new AampDRMLicenseManager(3, mAamp);
		mAamp->mDRMLicenseManager = mLicenseManager;

		const std::string debugLevel{"test_level"};
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
			.WillOnce(Return(debugLevel));
		mPlayer = new AAMPGstPlayer{mAamp, nullptr};
	}

	void TearDown() override
	{
		delete mPlayer;
		mPlayer = nullptr;

		mAamp->mDRMLicenseManager = nullptr;
		delete mLicenseManager;
		delete mAamp;
		delete mConfig;

		g_mockAampLicenseManager.reset();
		g_mockPrivateInstanceAAMP.reset();
		g_mockAampConfig.reset();
		g_mockGLib.reset();
		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;
		g_mockAampUtils.reset();
	}

	void SendBusError(const std::string &msg)
	{
		BusEventData event{};
		event.msgType = MESSAGE_ERROR;
		event.msg = msg;
		ASSERT_TRUE(mPlayer->playerInstance->busMessageCallback);
		mPlayer->playerInstance->busMessageCallback(event);
	}
};

/**
 * @brief The Rialto decrypt-drop message flags the active sessions and is reported as a DRM decrypt
 *        failure (retry enabled), not as a generic pipeline error.
 */
TEST_F(AAMPGstPlayerBusErrorTests, RialtoDecryptDropFlagsSessionsAndReportsDecryptFailure)
{
	const std::string desc = "GstPipeline Error:Rialto dropped a frame that failed to decrypt";

	testing::Sequence seq;
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).InSequence(seq);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED, StrEq(desc), true, _, _, _, _)).InSequence(seq);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, _, _, _, _, _, _)).Times(0);

	SendBusError("Rialto dropped a frame that failed to decrypt");
}

/**
 * @brief Without a license manager the decrypt failure is still reported.
 */
TEST_F(AAMPGstPlayerBusErrorTests, RialtoDecryptDropWithoutLicenseManagerStillReports)
{
	mAamp->mDRMLicenseManager = nullptr;

	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED, _, true, _, _, _, _));

	SendBusError("Rialto dropped a frame that failed to decrypt");
}

/**
 * @brief Regression: the pre-existing branches do not touch the session cache and keep
 *        their original reporting.
 */
TEST_F(AAMPGstPlayerBusErrorTests, VideoDecodeErrorUnchanged)
{
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, _, false, _, _, _, _));

	SendBusError("video decode error");
}

TEST_F(AAMPGstPlayerBusErrorTests, HdcpComplianceErrorUnchanged)
{
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR, _, false, _, _, _, _));

	SendBusError("HDCP Compliance Check Failure");
}

TEST_F(AAMPGstPlayerBusErrorTests, InternalDataStreamErrorSchedulesRetuneWhenConfigured)
{
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RetuneForGSTError)).WillByDefault(Return(true));

	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);

	SendBusError("Internal data stream error");
}

TEST_F(AAMPGstPlayerBusErrorTests, InternalDataStreamErrorFallsToGenericWhenNotConfigured)
{
	ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_RetuneForGSTError)).WillByDefault(Return(false));

	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, ScheduleRetune(_, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, _, true, _, _, _, _));

	SendBusError("Internal data stream error");
}

TEST_F(AAMPGstPlayerBusErrorTests, H264ParseErrorOnlyLogged)
{
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, ScheduleRetune(_, _)).Times(0);

	SendBusError("Error parsing H.264 stream");
}

TEST_F(AAMPGstPlayerBusErrorTests, CorruptFileErrorUnchanged)
{
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, _, false, _, _, _, _));

	SendBusError("This file is corrupt and cannot be played");
}

TEST_F(AAMPGstPlayerBusErrorTests, UnknownErrorStillGenericPipelineError)
{
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, _, true, _, _, _, _));

	SendBusError("some other pipeline error");
}
