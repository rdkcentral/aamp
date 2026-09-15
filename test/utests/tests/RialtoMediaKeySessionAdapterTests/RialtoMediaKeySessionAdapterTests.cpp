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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "RialtoMediaKeySessionAdapter.h"
#include "MockDrmHelper.h"
#include "MockRialtoMediaKeys.h"

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

#include <thread>
#include <chrono>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::SaveArg;

class MockDrmCallbacks : public DrmCallbacks
{
public:
	MOCK_METHOD(void, Individualization, (const std::string& payload), (override));
	MOCK_METHOD(void, LicenseRenewal, (DrmHelperPtr drmHelper, void* userData), (override));
};

class RialtoMediaKeySessionAdapterTest : public ::testing::Test
{
protected:
	static constexpr int32_t TEST_SESSION_ID = 10;
	const std::string m_keySystem = "com.widevine.alpha";

	std::shared_ptr<NiceMock<MockDrmHelper>> m_mockHelper;
	std::shared_ptr<NiceMock<MockMediaKeysFactory>> m_mockFactory;
	NiceMock<MockMediaKeys>* m_mockMediaKeysRaw = nullptr;
	NiceMock<MockDrmCallbacks> m_mockCallbacks;

	// Captured IMediaKeysClient for simulating callbacks
	std::weak_ptr<firebolt::rialto::IMediaKeysClient> m_capturedClient;

	void SetUp() override
	{
		m_mockHelper = std::make_shared<NiceMock<MockDrmHelper>>();
		m_mockFactory = std::make_shared<NiceMock<MockMediaKeysFactory>>();

		ON_CALL(*m_mockHelper, ocdmSystemId()).WillByDefault(testing::ReturnRef(m_keySystem));
		ON_CALL(*m_mockHelper, keyProcessTimeout()).WillByDefault(Return(5000));
		ON_CALL(*m_mockHelper, licenseGenerateTimeout()).WillByDefault(Return(5000));
	}

	std::unique_ptr<RialtoMediaKeySessionAdapter> createAdapter()
	{
		auto mockMediaKeys = std::make_unique<NiceMock<MockMediaKeys>>();
		m_mockMediaKeysRaw = mockMediaKeys.get();

		EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
			.WillOnce(Return(testing::ByMove(std::move(mockMediaKeys))));

		// Capture the client weak_ptr for callback simulation
		ON_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
			.WillByDefault(DoAll(
				Invoke([this](firebolt::rialto::KeySessionType,
				              std::weak_ptr<firebolt::rialto::IMediaKeysClient> client,
				              int32_t&) {
					m_capturedClient = client;
				}),
				SetArgReferee<2>(TEST_SESSION_ID),
				Return(firebolt::rialto::MediaKeyErrorStatus::OK)));

		ON_CALL(*m_mockMediaKeysRaw, generateRequest(_, _, _, _))
			.WillByDefault(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

		auto system = std::make_unique<RialtoMediaKeySystem>(m_keySystem, m_mockFactory);
		return std::make_unique<RialtoMediaKeySessionAdapter>(
			m_mockHelper, std::move(system), &m_mockCallbacks);
	}

	void simulateLicenseRequest(const std::string& url, const std::vector<uint8_t>& challenge)
	{
		auto client = m_capturedClient.lock();
		if (client)
		{
			client->onLicenseRequest(TEST_SESSION_ID, challenge, url);
		}
	}

	void simulateKeyStatusUsable(const std::vector<uint8_t>& keyId)
	{
		auto client = m_capturedClient.lock();
		if (client)
		{
			firebolt::rialto::KeyStatusVector statuses;
			statuses.push_back({keyId, firebolt::rialto::KeyStatus::USABLE});
			client->onKeyStatusesChanged(TEST_SESSION_ID, statuses);
		}
	}
};

TEST_F(RialtoMediaKeySessionAdapterTest, InitialState)
{
	auto adapter = createAdapter();
	EXPECT_EQ(KEY_INIT, adapter->getState());
	EXPECT_EQ(-1, adapter->getMediaKeySessionId());
}

TEST_F(RialtoMediaKeySessionAdapterTest, GenerateDRMSessionSuccess)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00, 0x01, 0x02};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	// Session should have been created — getMediaKeySessionId should return the ID.
	EXPECT_EQ(TEST_SESSION_ID, adapter->getMediaKeySessionId());
}

TEST_F(RialtoMediaKeySessionAdapterTest, GenerateDRMSessionFailsWithNullSystem)
{
	// Create adapter with a system that has no IMediaKeys (factory returns null)
	EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
		.WillOnce(Return(testing::ByMove(nullptr)));

	auto system = std::make_unique<RialtoMediaKeySystem>(m_keySystem, m_mockFactory);
	auto adapter = std::make_unique<RialtoMediaKeySessionAdapter>(
		m_mockHelper, std::move(system), &m_mockCallbacks);

	const uint8_t initData[] = {0x00};
	std::string customData;
	adapter->generateDRMSession(initData, sizeof(initData), customData);

	EXPECT_EQ(KEY_ERROR, adapter->getState());
}

TEST_F(RialtoMediaKeySessionAdapterTest, GenerateKeyRequestReceivesChallenge)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	// Simulate the Rialto server sending a license request callback
	const std::string url = "https://license.example.com";
	const std::vector<uint8_t> challenge = {'c', 'h', 'a', 'l', 'l', 'e', 'n', 'g', 'e'};
	simulateLicenseRequest(url, challenge);

	std::string destUrl;
	DrmData* result = adapter->generateKeyRequest(destUrl, 1000);

	ASSERT_NE(nullptr, result);
	EXPECT_EQ(url, destUrl);
	EXPECT_EQ(KEY_PENDING, adapter->getState());

	delete result;
}

TEST_F(RialtoMediaKeySessionAdapterTest, GenerateKeyRequestTimesOut)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	// Don't simulate any callback — should timeout
	std::string destUrl;
	DrmData* result = adapter->generateKeyRequest(destUrl, 50);

	EXPECT_EQ(nullptr, result);
}

TEST_F(RialtoMediaKeySessionAdapterTest, ProcessDRMKeySuccess)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	// Simulate challenge
	simulateLicenseRequest("https://lic.test", {'l', 'i', 'c'});

	// processDRMKey
	EXPECT_CALL(*m_mockMediaKeysRaw, updateSession(TEST_SESSION_ID, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	DrmData key("license-response", 16);

	// Simulate key becoming usable (in separate thread to avoid deadlock)
	const std::vector<uint8_t> keyId = {0xAA, 0xBB};
	std::thread callbackThread([this, &keyId]() {
		simulateKeyStatusUsable(keyId);
	});

	int result = adapter->processDRMKey(&key, 2000);
	callbackThread.join();

	EXPECT_EQ(0, result);
	EXPECT_EQ(KEY_READY, adapter->getState());
}

TEST_F(RialtoMediaKeySessionAdapterTest, ProcessDRMKeyUpdateFails)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	EXPECT_CALL(*m_mockMediaKeysRaw, updateSession(TEST_SESSION_ID, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	DrmData key("response", 8);
	int result = adapter->processDRMKey(&key, 100);

	EXPECT_NE(0, result);
	EXPECT_EQ(KEY_ERROR, adapter->getState());
}

TEST_F(RialtoMediaKeySessionAdapterTest, ClearDecryptContext)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	EXPECT_CALL(*m_mockMediaKeysRaw, closeKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));
	EXPECT_CALL(*m_mockMediaKeysRaw, releaseKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	adapter->clearDecryptContext();

	EXPECT_EQ(KEY_INIT, adapter->getState());
	EXPECT_EQ(-1, adapter->getMediaKeySessionId());
}

TEST_F(RialtoMediaKeySessionAdapterTest, LicenseRenewalCallbackRouted)
{
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;

	adapter->generateDRMSession(initData, sizeof(initData), customData);

	EXPECT_CALL(m_mockCallbacks, LicenseRenewal(_, _)).Times(1);

	// Simulate license renewal from Rialto
	auto client = m_capturedClient.lock();
	ASSERT_NE(nullptr, client);
	const std::vector<unsigned char> renewalMsg = {'r', 'e', 'n', 'e', 'w'};
	client->onLicenseRenewal(TEST_SESSION_ID, renewalMsg);
}
