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
 * @file AampDrmBridgeTestCases.cpp
 * @brief L1 unit tests for AampDrmBridge.
 *
 * The license manager is the fake from the fakes library; its system-id
 * createDrmSession() overload delegates to MockAampLicenseManager.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>

#include "AampDrmBridge.h"
#include "priv_aamp.h"
#include "DrmSessionManager.h"
#include "MockAampLicManager.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace
{
/// Session that only reports a fixed media key session id.
class TestDrmSession : public DrmSession
{
public:
	explicit TestDrmSession(int32_t mksId) : DrmSession("test-key-system"), m_mksId(mksId) {}

	void generateDRMSession(const uint8_t *, uint32_t, std::string &) override {}
	DrmData *generateKeyRequest(std::string &, uint32_t) override { return nullptr; }
	int processDRMKey(DrmData *, uint32_t) override { return 0; }
	KeyState getState() override { return KEY_READY; }
	void clearDecryptContext() override {}
	int32_t getMediaKeySessionId() const override { return m_mksId; }

private:
	int32_t m_mksId;
};

constexpr const char *kSystemId = "test-system-id";
constexpr uint16_t kMaxInitDataLen = 65535;
} // namespace

class AampDrmBridgeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		g_mockAampLicenseManager = std::make_shared<NiceMock<MockAampLicenseManager>>();
		m_sessionManager = std::make_unique<DrmSessionManager>(1, nullptr, nullptr);
		m_licenseManager = std::make_unique<AampDRMLicenseManager>(1, &m_aamp);
		m_licenseManager->mDrmSessionManager = m_sessionManager.get();
		m_aamp.mDRMLicenseManager = m_licenseManager.get();
		m_bridge = std::make_unique<AampDrmBridge>(&m_aamp);
	}

	void TearDown() override
	{
		m_bridge.reset();
		m_aamp.mDRMLicenseManager = nullptr;
		m_licenseManager.reset();
		m_sessionManager.reset();
		g_mockAampLicenseManager.reset();
	}

	PrivateInstanceAAMP m_aamp{};
	std::unique_ptr<DrmSessionManager> m_sessionManager;
	std::unique_ptr<AampDRMLicenseManager> m_licenseManager;
	std::unique_ptr<AampDrmBridge> m_bridge;
};

TEST_F(AampDrmBridgeTest, CreateSession_PassesMediaFormatOfCurrentTune)
{
	/**
	 * @brief Regression (L5): the format was fixed to DASH, which the DRM helpers
	 *        use to choose behaviour, so HLS content got the DASH treatment.
	 */
	const std::vector<uint8_t> initData(16, 0x11);
	const MediaFormat kFormats[] = {eMEDIAFORMAT_HLS, eMEDIAFORMAT_DASH, eMEDIAFORMAT_HLS_MP4};

	for (const MediaFormat format : kFormats)
	{
		m_aamp.mMediaFormat = format;
		TestDrmSession session(5);
		EXPECT_CALL(*g_mockAampLicenseManager,
			createDrmSessionBySystemId(StrEq(kSystemId), format, _,
				static_cast<unsigned short>(initData.size()),
				static_cast<int>(eMEDIATYPE_VIDEO)))
			.WillOnce(Return(&session));

		EXPECT_EQ(5, m_bridge->createSession(
			kSystemId, initData.data(), initData.size(), eMEDIATYPE_VIDEO))
			<< "mediaFormat=" << format;
	}
}

TEST_F(AampDrmBridgeTest, CreateSession_InitDataOverLimit_IsRejectedNotTruncated)
{
	/**
	 * @brief Regression (L5): the length was cast to 16 bits, so init data over
	 *        64 KB was silently truncated before the license request.
	 */
	const std::vector<uint8_t> initData(static_cast<size_t>(kMaxInitDataLen) + 1, 0x22);

	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSessionBySystemId(_, _, _, _, _))
		.Times(0);

	EXPECT_EQ(-1, m_bridge->createSession(
		kSystemId, initData.data(), initData.size(), eMEDIATYPE_VIDEO));
}

TEST_F(AampDrmBridgeTest, CreateSession_InitDataAtLimit_IsForwardedWhole)
{
	const std::vector<uint8_t> initData(kMaxInitDataLen, 0x33);
	TestDrmSession session(9);

	EXPECT_CALL(*g_mockAampLicenseManager,
		createDrmSessionBySystemId(_, _, _, kMaxInitDataLen, _))
		.WillOnce(Return(&session));

	EXPECT_EQ(9, m_bridge->createSession(
		kSystemId, initData.data(), initData.size(), eMEDIATYPE_AUDIO));
}

TEST_F(AampDrmBridgeTest, CreateSession_SessionCreationFails_ReturnsMinusOne)
{
	const std::vector<uint8_t> initData(16, 0x44);

	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSessionBySystemId(_, _, _, _, _))
		.WillOnce(Return(nullptr));

	EXPECT_EQ(-1, m_bridge->createSession(
		kSystemId, initData.data(), initData.size(), eMEDIATYPE_VIDEO));
}

TEST_F(AampDrmBridgeTest, CreateSession_NoLicenseManager_ReturnsMinusOne)
{
	const std::vector<uint8_t> initData(16, 0x55);
	m_aamp.mDRMLicenseManager = nullptr;

	EXPECT_CALL(*g_mockAampLicenseManager, createDrmSessionBySystemId(_, _, _, _, _))
		.Times(0);

	EXPECT_EQ(-1, m_bridge->createSession(
		kSystemId, initData.data(), initData.size(), eMEDIATYPE_VIDEO));
}
