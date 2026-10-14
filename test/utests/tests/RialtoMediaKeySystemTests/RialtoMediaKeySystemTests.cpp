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

#include <chrono>
#include <future>

#include "RialtoMediaKeySystem.h"
#include "MockRialtoMediaKeys.h"

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;

class RialtoMediaKeySystemTest : public ::testing::Test
{
protected:
	static constexpr int32_t TEST_SESSION_ID = 7;
	const std::string m_keySystem = "com.widevine.alpha";

	std::shared_ptr<NiceMock<MockMediaKeysFactory>> m_mockFactory;
	NiceMock<MockMediaKeys>* m_mockMediaKeysRaw = nullptr;

	void SetUp() override
	{
		m_mockFactory = std::make_shared<NiceMock<MockMediaKeysFactory>>();
	}

	/// Create a session and capture the client the system registered with Rialto.
	std::unique_ptr<RialtoMediaKeySession> createSessionCapturingClient(
		RialtoMediaKeySystem& system,
		const RialtoSessionCallbacks& callbacks,
		std::weak_ptr<firebolt::rialto::IMediaKeysClient>& capturedClient)
	{
		EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
			.WillOnce(DoAll(
				Invoke([&capturedClient](firebolt::rialto::KeySessionType,
				              std::weak_ptr<firebolt::rialto::IMediaKeysClient> client,
				              int32_t&)
				{
					capturedClient = client;
				}),
				SetArgReferee<2>(TEST_SESSION_ID),
				Return(firebolt::rialto::MediaKeyErrorStatus::OK)));
		EXPECT_CALL(*m_mockMediaKeysRaw, generateRequest(_, _, _, _))
			.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));
		const uint8_t initData[] = {0x00};
		return system.createSession("cenc", initData, sizeof(initData), callbacks);
	}

	std::unique_ptr<RialtoMediaKeySystem> createSystemWithMediaKeys()
	{
		auto mockMediaKeys = std::make_unique<NiceMock<MockMediaKeys>>();
		m_mockMediaKeysRaw = mockMediaKeys.get();

		EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
			.WillOnce(Return(testing::ByMove(std::move(mockMediaKeys))));

		return std::make_unique<RialtoMediaKeySystem>(m_keySystem, m_mockFactory);
	}
};

TEST_F(RialtoMediaKeySystemTest, ConstructionWithNullFactory)
{
	RialtoMediaKeySystem system(m_keySystem, nullptr);
	EXPECT_FALSE(system.isValid());
}

TEST_F(RialtoMediaKeySystemTest, ConstructionFactoryReturnsNull)
{
	EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
		.WillOnce(Return(testing::ByMove(nullptr)));

	RialtoMediaKeySystem system(m_keySystem, m_mockFactory);
	EXPECT_FALSE(system.isValid());
}

TEST_F(RialtoMediaKeySystemTest, ConstructionSuccess)
{
	auto system = createSystemWithMediaKeys();
	EXPECT_TRUE(system->isValid());
}

TEST_F(RialtoMediaKeySystemTest, CreateSessionSuccess)
{
	auto system = createSystemWithMediaKeys();
	const uint8_t initData[] = {0x00, 0x01, 0x02, 0x03};

	EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(
		firebolt::rialto::KeySessionType::TEMPORARY, _, _))
		.WillOnce(DoAll(
			SetArgReferee<2>(TEST_SESSION_ID),
			Return(firebolt::rialto::MediaKeyErrorStatus::OK)));

	EXPECT_CALL(*m_mockMediaKeysRaw, generateRequest(TEST_SESSION_ID,
		firebolt::rialto::InitDataType::CENC, _, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	RialtoSessionCallbacks callbacks;
	auto session = system->createSession("cenc", initData, sizeof(initData), callbacks);

	ASSERT_NE(nullptr, session);
	EXPECT_EQ(TEST_SESSION_ID, session->getMediaKeySessionId());
}

TEST_F(RialtoMediaKeySystemTest, CreateSessionFailsOnCreateKeySession)
{
	auto system = createSystemWithMediaKeys();
	const uint8_t initData[] = {0x00};

	EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	RialtoSessionCallbacks callbacks;
	auto session = system->createSession("cenc", initData, sizeof(initData), callbacks);

	EXPECT_EQ(nullptr, session);
}

TEST_F(RialtoMediaKeySystemTest, CreateSessionFailsOnGenerateRequest)
{
	auto system = createSystemWithMediaKeys();
	const uint8_t initData[] = {0x00};

	EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
		.WillOnce(DoAll(
			SetArgReferee<2>(TEST_SESSION_ID),
			Return(firebolt::rialto::MediaKeyErrorStatus::OK)));

	EXPECT_CALL(*m_mockMediaKeysRaw, generateRequest(TEST_SESSION_ID, _, _, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	// Should clean up session on failure
	EXPECT_CALL(*m_mockMediaKeysRaw, releaseKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	RialtoSessionCallbacks callbacks;
	auto session = system->createSession("cenc", initData, sizeof(initData), callbacks);

	EXPECT_EQ(nullptr, session);
}

TEST_F(RialtoMediaKeySystemTest, CreateSessionWebmInitDataType)
{
	auto system = createSystemWithMediaKeys();
	const uint8_t initData[] = {0x00};

	EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
		.WillOnce(DoAll(
			SetArgReferee<2>(TEST_SESSION_ID),
			Return(firebolt::rialto::MediaKeyErrorStatus::OK)));

	EXPECT_CALL(*m_mockMediaKeysRaw, generateRequest(TEST_SESSION_ID,
		firebolt::rialto::InitDataType::WEBM, _, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	RialtoSessionCallbacks callbacks;
	auto session = system->createSession("webm", initData, sizeof(initData), callbacks);

	ASSERT_NE(nullptr, session);
}

TEST_F(RialtoMediaKeySystemTest, OnLicenseRequestRoutesToCallback)
{
	auto system = createSystemWithMediaKeys();
	const uint8_t initData[] = {0x00};

	std::weak_ptr<firebolt::rialto::IMediaKeysClient> capturedClient;
	EXPECT_CALL(*m_mockMediaKeysRaw, createKeySession(_, _, _))
		.WillOnce(DoAll(
 			Invoke([&capturedClient](firebolt::rialto::KeySessionType,
 			              std::weak_ptr<firebolt::rialto::IMediaKeysClient> client,
 			              int32_t&)
 			{
 				capturedClient = client;
 			}),
			SetArgReferee<2>(TEST_SESSION_ID),
			Return(firebolt::rialto::MediaKeyErrorStatus::OK)));

	EXPECT_CALL(*m_mockMediaKeysRaw, generateRequest(_, _, _, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	bool challengeReceived = false;
	std::string receivedUrl;

	RialtoSessionCallbacks callbacks;
	callbacks.onChallenge = [&](const char* url, const uint8_t* challenge, uint16_t size)
	{
		challengeReceived = true;
		receivedUrl = url;
	};

	auto session = system->createSession("cenc", initData, sizeof(initData), callbacks);
	ASSERT_NE(nullptr, session);

 	auto client = capturedClient.lock();
 	ASSERT_NE(nullptr, client);
 	const std::string url = "https://license.example.com";
 	const std::vector<unsigned char> challengeMsg = {'c', 'h', 'a', 'l'};
 	client->onLicenseRequest(TEST_SESSION_ID, challengeMsg, url);
 	EXPECT_TRUE(challengeReceived);
 	EXPECT_EQ(receivedUrl, url);
}

TEST_F(RialtoMediaKeySystemTest, CreateSessionOnInvalidSystem)
{
	EXPECT_CALL(*m_mockFactory, createMediaKeys(m_keySystem))
		.WillOnce(Return(testing::ByMove(nullptr)));

	RialtoMediaKeySystem system(m_keySystem, m_mockFactory);
	EXPECT_FALSE(system.isValid());

	const uint8_t initData[] = {0x00};
	RialtoSessionCallbacks callbacks;
	auto session = system.createSession("cenc", initData, sizeof(initData), callbacks);
	EXPECT_EQ(nullptr, session);
}

TEST_F(RialtoMediaKeySystemTest, OnKeyStatusesChanged_UpdatesSessionThenRunsCallbacks)
{
	auto system = createSystemWithMediaKeys();
	const std::vector<uint8_t> keyA = {0xA1, 0xA2};
	const std::vector<uint8_t> keyB = {0xB1};

	RialtoMediaKeySession* sessionPtr = nullptr;
	std::vector<bool> usableWhenNotified;
	int keysUpdatedCalls = 0;
	RialtoSessionCallbacks callbacks;
	callbacks.onKeyUpdate = [&](const uint8_t* key, uint8_t size)
	{
		usableWhenNotified.push_back(sessionPtr->isKeyUsable(key, size));
	};
	callbacks.onKeysUpdated = [&] { ++keysUpdatedCalls; };

	std::weak_ptr<firebolt::rialto::IMediaKeysClient> weakClient;
	auto session = createSessionCapturingClient(*system, callbacks, weakClient);
	ASSERT_NE(nullptr, session);
	sessionPtr = session.get();
	auto client = weakClient.lock();
	ASSERT_NE(nullptr, client);

	client->onKeyStatusesChanged(TEST_SESSION_ID,
		{{keyA, firebolt::rialto::KeyStatus::USABLE},
		 {keyB, firebolt::rialto::KeyStatus::USABLE}});

	EXPECT_EQ(usableWhenNotified, (std::vector<bool>{true, true}));
	EXPECT_EQ(keysUpdatedCalls, 1);
}

TEST_F(RialtoMediaKeySystemTest, OnKeyStatusesChanged_CallbackMayDestructItsSession_DoesNotDeadlock)
{
	/**
	 * @brief Regression (M11): onKeyStatusesChanged ran the callbacks while
	 *        holding the client mutex, so a callback that destructs its own
	 *        session (which deregisters under that mutex) deadlocked.
	 */
	auto system = createSystemWithMediaKeys();
	RialtoMediaKeySession* sessionPtr = nullptr;
	RialtoSessionCallbacks callbacks;
	callbacks.onKeysUpdated = [&] { sessionPtr->destruct(); };

	std::weak_ptr<firebolt::rialto::IMediaKeysClient> weakClient;
	auto session = createSessionCapturingClient(*system, callbacks, weakClient);
	ASSERT_NE(nullptr, session);
	sessionPtr = session.get();
	auto client = weakClient.lock();
	ASSERT_NE(nullptr, client);

	auto result = std::async(std::launch::async, [&] {
		client->onKeyStatusesChanged(TEST_SESSION_ID,
			{{std::vector<uint8_t>{0x01}, firebolt::rialto::KeyStatus::USABLE}});
	});
	ASSERT_EQ(result.wait_for(std::chrono::seconds(5)), std::future_status::ready)
		<< "onKeyStatusesChanged deadlocked in a callback that deregisters";
}

TEST_F(RialtoMediaKeySystemTest, OnKeyStatusesChanged_AfterSessionDroppedWithoutDestruct_IsIgnored)
{
	/**
	 * @brief Regression (M11): the client kept a raw pointer to the session and
	 *        the session destructor did not deregister, so a key-status event
	 *        arriving after the session was dropped without destruct() wrote to
	 *        freed memory.  Run under AddressSanitizer to see the old fault.
	 */
	auto system = createSystemWithMediaKeys();
	int keyUpdates = 0;
	RialtoSessionCallbacks callbacks;
	callbacks.onKeyUpdate = [&](const uint8_t*, uint8_t) { ++keyUpdates; };

	std::weak_ptr<firebolt::rialto::IMediaKeysClient> weakClient;
	auto session = createSessionCapturingClient(*system, callbacks, weakClient);
	ASSERT_NE(nullptr, session);
	auto client = weakClient.lock();
	ASSERT_NE(nullptr, client);

	session.reset();

	client->onKeyStatusesChanged(TEST_SESSION_ID,
		{{std::vector<uint8_t>{0x01}, firebolt::rialto::KeyStatus::USABLE}});

	EXPECT_EQ(keyUpdates, 0) << "a dropped session's callbacks must not run";
}
