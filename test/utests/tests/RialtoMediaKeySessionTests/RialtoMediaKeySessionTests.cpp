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

#include "RialtoMediaKeySession.h"
#include "MockRialtoMediaKeys.h"

int main(int argc, char **argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class RialtoMediaKeySessionTest : public ::testing::Test
{
protected:
	static constexpr int32_t TEST_SESSION_ID = 42;

	NiceMock<MockMediaKeys> m_mockMediaKeys;
	bool m_deregistered = false;

	std::unique_ptr<RialtoMediaKeySession> createSession()
	{
		auto deregister = [this](int32_t id)
		{
			m_deregistered = true;
		};
		return std::make_unique<RialtoMediaKeySession>(
			TEST_SESSION_ID, m_mockMediaKeys, std::move(deregister));
	}
};

TEST_F(RialtoMediaKeySessionTest, GetMediaKeySessionId)
{
	auto session = createSession();
	EXPECT_EQ(TEST_SESSION_ID, session->getMediaKeySessionId());
}

TEST_F(RialtoMediaKeySessionTest, UpdateSuccess)
{
	auto session = createSession();
	const uint8_t keyMsg[] = {0x01, 0x02, 0x03};

	EXPECT_CALL(m_mockMediaKeys, updateSession(TEST_SESSION_ID, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	EXPECT_TRUE(session->update(keyMsg, sizeof(keyMsg)));
}

TEST_F(RialtoMediaKeySessionTest, UpdateFailure)
{
	auto session = createSession();
	const uint8_t keyMsg[] = {0x01, 0x02, 0x03};

	EXPECT_CALL(m_mockMediaKeys, updateSession(TEST_SESSION_ID, _))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	EXPECT_FALSE(session->update(keyMsg, sizeof(keyMsg)));
}

TEST_F(RialtoMediaKeySessionTest, CloseSuccess)
{
	auto session = createSession();

	EXPECT_CALL(m_mockMediaKeys, closeKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	EXPECT_TRUE(session->close());
}

TEST_F(RialtoMediaKeySessionTest, CloseFailure)
{
	auto session = createSession();

	EXPECT_CALL(m_mockMediaKeys, closeKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	EXPECT_FALSE(session->close());
}

TEST_F(RialtoMediaKeySessionTest, DestructSuccess)
{
	auto session = createSession();

	EXPECT_CALL(m_mockMediaKeys, releaseKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::OK));

	EXPECT_TRUE(session->destruct());
	EXPECT_TRUE(m_deregistered);
}

TEST_F(RialtoMediaKeySessionTest, DestructCallsDeregisterEvenOnFailure)
{
	auto session = createSession();

	EXPECT_CALL(m_mockMediaKeys, releaseKeySession(TEST_SESSION_ID))
		.WillOnce(Return(firebolt::rialto::MediaKeyErrorStatus::FAIL));

	EXPECT_FALSE(session->destruct());
	EXPECT_TRUE(m_deregistered);
}

TEST_F(RialtoMediaKeySessionTest, IsKeyUsableInitiallyFalse)
{
	auto session = createSession();
	const uint8_t keyId[] = {0xAA, 0xBB};

	EXPECT_FALSE(session->isKeyUsable(keyId, sizeof(keyId)));
}

TEST_F(RialtoMediaKeySessionTest, IsKeyUsableAfterUpdate)
{
	auto session = createSession();
	const std::vector<uint8_t> keyId = {0xAA, 0xBB};

	session->updateKeyStatus(keyId, firebolt::rialto::KeyStatus::USABLE);

	EXPECT_TRUE(session->isKeyUsable(keyId.data(), static_cast<uint8_t>(keyId.size())));
}

TEST_F(RialtoMediaKeySessionTest, IsKeyUsableReturnsFalseForExpired)
{
	auto session = createSession();
	const std::vector<uint8_t> keyId = {0xAA, 0xBB};

	session->updateKeyStatus(keyId, firebolt::rialto::KeyStatus::EXPIRED);

	EXPECT_FALSE(session->isKeyUsable(keyId.data(), static_cast<uint8_t>(keyId.size())));
}

TEST_F(RialtoMediaKeySessionTest, IsKeyOutputRestricted)
{
	auto session = createSession();
	const std::vector<uint8_t> keyId = {0xCC, 0xDD};

	session->updateKeyStatus(keyId, firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED);

	EXPECT_TRUE(session->isKeyOutputRestricted(keyId.data(), static_cast<uint8_t>(keyId.size())));
}

TEST_F(RialtoMediaKeySessionTest, MultipleKeysTrackedIndependently)
{
	auto session = createSession();
	const std::vector<uint8_t> keyId1 = {0x01};
	const std::vector<uint8_t> keyId2 = {0x02};

	session->updateKeyStatus(keyId1, firebolt::rialto::KeyStatus::USABLE);
	session->updateKeyStatus(keyId2, firebolt::rialto::KeyStatus::EXPIRED);

	EXPECT_TRUE(session->isKeyUsable(keyId1.data(), static_cast<uint8_t>(keyId1.size())));
	EXPECT_FALSE(session->isKeyUsable(keyId2.data(), static_cast<uint8_t>(keyId2.size())));
}
