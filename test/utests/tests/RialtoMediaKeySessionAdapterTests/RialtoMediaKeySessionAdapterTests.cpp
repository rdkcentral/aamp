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

#include <atomic>
#include <chrono>
#include <thread>

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

	// The key status can only follow the license update, so deliver it from
	// the update call (on another thread, as Rialto's event thread would).
	const std::vector<uint8_t> keyId = {0xAA, 0xBB};
	std::thread callbackThread;
	EXPECT_CALL(*m_mockMediaKeysRaw, updateSession(TEST_SESSION_ID, _))
		.WillOnce(Invoke([&](int32_t, const std::vector<uint8_t>&) {
			callbackThread = std::thread([this, &keyId]() {
				simulateKeyStatusUsable(keyId);
			});
			return firebolt::rialto::MediaKeyErrorStatus::OK;
		}));

	DrmData key("license-response", 16);

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

TEST_F(RialtoMediaKeySessionAdapterTest, WaitForState_StateSetJustAfterPredicateCheck_IsNotMissed)
{
	/**
	 * @brief Regression (M12): the state was stored and the condition variable
	 *        notified without holding the waiters' mutex, so a waitForState()
	 *        that had tested its predicate but not yet blocked missed the
	 *        notification and slept until its timeout.  The race window is
	 *        narrow, so this loops.  wait_for() returns the predicate's value
	 *        when it times out, so a miss does not show in the result: the
	 *        waiter simply fails to return promptly.  The waiter therefore has
	 *        a long timeout and is given a couple of seconds to finish.
	 */
	int missed = 0;
	for (int i = 0; i < 150; ++i)
	{
		// A system with no IMediaKeys makes generateDRMSession() set KEY_ERROR.
		EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
			.WillOnce(Return(testing::ByMove(nullptr)));
		auto system = std::make_unique<RialtoMediaKeySystem>(m_keySystem, m_mockFactory);
		auto adapter = std::make_unique<RialtoMediaKeySessionAdapter>(
			m_mockHelper, std::move(system), &m_mockCallbacks);

		std::atomic<bool> waiting{false};
		std::atomic<bool> finished{false};
		std::thread waiter([&] {
			waiting = true;
			adapter->waitForState(KEY_ERROR, 30000);
			finished = true;
		});
		while (!waiting.load())
		{
			std::this_thread::yield();
		}
		const uint8_t initData[] = {0x00};
		std::string customData;
		adapter->generateDRMSession(initData, sizeof(initData), customData);

		const auto deadline =
			std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!finished.load() && std::chrono::steady_clock::now() < deadline)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (!finished.load())
		{
			++missed;
			// Release the waiter that slept through the change.
			adapter->generateDRMSession(initData, sizeof(initData), customData);
		}
		waiter.join();
	}
	EXPECT_EQ(missed, 0) << "waitForState slept through a state change";
}

TEST_F(RialtoMediaKeySessionAdapterTest, ClearDecryptContext_DuringProcessDRMKey_IsSafe)
{
	/**
	 * @brief Regression (M12): m_session was read by processDRMKey() and
	 *        getMediaKeySessionId() while clearDecryptContext() reset it.
	 *        Run under a thread sanitizer to see the old race.
	 */
	for (int i = 0; i < 40; ++i)
	{
		auto adapter = createAdapter();
		const uint8_t initData[] = {0x00};
		std::string customData;
		adapter->generateDRMSession(initData, sizeof(initData), customData);

		ON_CALL(*m_mockMediaKeysRaw, updateSession(_, _))
			.WillByDefault(Return(firebolt::rialto::MediaKeyErrorStatus::OK));
		ON_CALL(*m_mockMediaKeysRaw, closeKeySession(_))
			.WillByDefault(Return(firebolt::rialto::MediaKeyErrorStatus::OK));
		ON_CALL(*m_mockMediaKeysRaw, releaseKeySession(_))
			.WillByDefault(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

		DrmData key("response", 8);
		std::atomic<bool> done{false};
		std::thread reader([&] {
			while (!done.load())
			{
				adapter->getMediaKeySessionId();
			}
		});
		std::thread processor([&] { adapter->processDRMKey(&key, 20); });
		std::thread status([&] { simulateKeyStatusUsable({0xAA}); });

		adapter->clearDecryptContext();
		processor.join();
		status.join();
		done = true;
		reader.join();
		EXPECT_EQ(adapter->getMediaKeySessionId(), -1);
	}
}

TEST_F(RialtoMediaKeySessionAdapterTest, ProcessDRMKey_SecondResponse_WaitsForItsOwnKeyStatus)
{
	/**
	 * @brief Regression (M12): m_keyStatusReceived was never cleared, so a
	 *        second license response did not wait for the status it produced
	 *        and read the previous one.  Here the second response restricts
	 *        the key; it must be reported, not the earlier USABLE status.
	 */
	auto adapter = createAdapter();
	const uint8_t initData[] = {0x00};
	std::string customData;
	adapter->generateDRMSession(initData, sizeof(initData), customData);

	const std::vector<uint8_t> keyId = {0xAA, 0xBB};
	int updates = 0;
	std::thread statusThread;
	ON_CALL(*m_mockMediaKeysRaw, updateSession(TEST_SESSION_ID, _))
		.WillByDefault(Invoke([&](int32_t, const std::vector<uint8_t>&) {
			const int n = ++updates;
			statusThread = std::thread([this, keyId, n]() {
				if (n == 1)
				{
					simulateKeyStatusUsable(keyId);
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					auto client = m_capturedClient.lock();
					ASSERT_NE(nullptr, client);
					firebolt::rialto::KeyStatusVector statuses;
					statuses.push_back({keyId, firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED});
					client->onKeyStatusesChanged(TEST_SESSION_ID, statuses);
				}
			});
			return firebolt::rialto::MediaKeyErrorStatus::OK;
		}));

	// Long timeouts: the old code never waits (it returns the stale status at
	// once), and a stalled test machine must not push the wait past them.
	DrmData key("response", 8);
	EXPECT_EQ(0, adapter->processDRMKey(&key, 20000));
	statusThread.join();
	EXPECT_EQ(KEY_READY, adapter->getState());

	EXPECT_EQ(HDCP_OUTPUT_PROTECTION_FAILURE, adapter->processDRMKey(&key, 20000));
	statusThread.join();
	EXPECT_EQ(KEY_ERROR, adapter->getState());
}
