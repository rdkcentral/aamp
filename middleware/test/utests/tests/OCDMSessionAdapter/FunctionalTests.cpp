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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "opencdmsessionadapter.h"
#include "MockDrmHelper.h"
#include "MockOpenCdm.h"
#include <cstring>
#include "_base64.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

std::shared_ptr<MockDrmHelper> drmHelper;

// For comparing memory buffers such as C-style arrays
MATCHER_P2(MemBufEq, buffer, elementCount, "")
{
	return std::memcmp(arg, buffer, elementCount * sizeof(buffer[0])) == 0;
}

class OCDMSessionAdapterTests : public ::testing::Test
{

protected:
	OCDMSessionAdapter *m_ocdmsessionadapter = nullptr;
	struct OpenCDMSystem *ocdmSystem = (OpenCDMSystem *)0x1234;
	std::string systemId = "com.microsoft.playready";

	void SetUp() override
	{
		drmHelper = std::make_shared<MockDrmHelper>();
		g_mockopencdm = new NiceMock<MockOpenCdm>();

		EXPECT_CALL(*drmHelper, ocdmSystemId()).WillOnce(ReturnRef(systemId));
		EXPECT_CALL(*g_mockopencdm, opencdm_create_system(MemBufEq(systemId.c_str(), systemId.length() + 1))).WillOnce(Return(ocdmSystem));

		m_ocdmsessionadapter = new OCDMSessionAdapter(drmHelper, nullptr);
	}

	void TearDown() override
	{
		delete m_ocdmsessionadapter;
		m_ocdmsessionadapter = nullptr;

		delete g_mockopencdm;
		g_mockopencdm = nullptr;

		drmHelper = nullptr;
	}

public:
};

TEST_F(OCDMSessionAdapterTests, generateDRMSession)
{
	uint8_t initData[] = {0, 1, 2, 3};
	uint8_t initDataLen = sizeof(initData);
	uint32_t f_cbInitData = 99;
	std::string customData = "Custom Data";
	const char *initDataType = "cenc";
	uint8_t initDataTypeLen = strlen(initDataType);

	((*g_mockopencdm).gmock_opencdm_construct_session(ocdmSystem, LicenseType::Temporary, MemBufEq(initDataType, initDataTypeLen), MemBufEq(initData, initDataLen), f_cbInitData, MemBufEq(customData.c_str(), customData.length()), customData.length(),_,_,_))(::testing::internal::GetWithoutMatchers(), nullptr) .InternalExpectedAt("/home/rekha/RDK/latest/l1_Final/aamp/middleware/test/utests/tests/OCDMSessionAdapter/FunctionalTests.cpp", 91, "*g_mockopencdm", "opencdm_construct_session(ocdmSystem, LicenseType::Temporary, MemBufEq(initDataType, initDataTypeLen), MemBufEq(initData, initDataLen), f_cbInitData, MemBufEq(customData.c_str(), customData.length()), customData.length(),_,_,_)").WillOnce(Return(ERROR_NONE));

	m_ocdmsessionadapter->generateDRMSession(initData, f_cbInitData, customData);
}

/**
 * @brief Test generateDRMSession with OCDM construction failure
 * 
 * Verifies that when opencdm_construct_session() fails, the KeyState is set to
 * KEY_ERROR_SESSION_CONSTRUCT_FAILED instead of generic KEY_ERROR.
 * This is part of PR VPLAY-11961 fix for proper DRM error reporting.
 */
TEST_F(OCDMSessionAdapterTests, generateDRMSession_ConstructionFailure_SetsSpecificErrorState)
{
	uint8_t initData[] = {0, 1, 2, 3};
	uint8_t initDataLen = sizeof(initData);
	uint32_t f_cbInitData = 99;
	std::string customData = "Custom Data";
	const char *initDataType = "cenc";
	uint8_t initDataTypeLen = strlen(initDataType);

	// Mock opencdm_construct_session to return error
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session(ocdmSystem, LicenseType::Temporary, 
		MemBufEq(initDataType, initDataTypeLen), MemBufEq(initData, initDataLen), 
		f_cbInitData, MemBufEq(customData.c_str(), customData.length()), 
		customData.length(), _, _, _))
		.WillOnce(Return(ERROR_FAIL));

	// Call generateDRMSession
	m_ocdmsessionadapter->generateDRMSession(initData, f_cbInitData, customData);

	// Verify that KeyState is set to KEY_ERROR_SESSION_CONSTRUCT_FAILED
	KeyState state = m_ocdmsessionadapter->getState();
	EXPECT_EQ(state, KEY_ERROR_SESSION_CONSTRUCT_FAILED);
}

/**
 * @brief Test generateDRMSession with invalid session error
 * 
 * Verifies that ERROR_INVALID_SESSION also results in KEY_ERROR_SESSION_CONSTRUCT_FAILED.
 */
TEST_F(OCDMSessionAdapterTests, generateDRMSession_InvalidSession_SetsSpecificErrorState)
{
	uint8_t initData[] = {0, 1, 2, 3};
	uint8_t initDataLen = sizeof(initData);
	uint32_t f_cbInitData = 99;
	std::string customData = "Custom Data";
	const char *initDataType = "cenc";
	uint8_t initDataTypeLen = strlen(initDataType);

	// Mock opencdm_construct_session to return ERROR_INVALID_SESSION
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session(ocdmSystem, LicenseType::Temporary, 
		MemBufEq(initDataType, initDataTypeLen), MemBufEq(initData, initDataLen), 
		f_cbInitData, MemBufEq(customData.c_str(), customData.length()), 
		customData.length(), _, _, _))
		.WillOnce(Return(ERROR_INVALID_SESSION));

	// Call generateDRMSession
	m_ocdmsessionadapter->generateDRMSession(initData, f_cbInitData, customData);

	// Verify that KeyState is set to KEY_ERROR_SESSION_CONSTRUCT_FAILED
	KeyState state = m_ocdmsessionadapter->getState();
	EXPECT_EQ(state, KEY_ERROR_SESSION_CONSTRUCT_FAILED);
}

/**
 * @brief Test generateDRMSession success sets correct state
 * 
 * Verifies that successful session construction does NOT set error state.
 */
TEST_F(OCDMSessionAdapterTests, generateDRMSession_Success_DoesNotSetErrorState)
{
	uint8_t initData[] = {0, 1, 2, 3};
	uint8_t initDataLen = sizeof(initData);
	uint32_t f_cbInitData = 99;
	std::string customData = "Custom Data";
	const char *initDataType = "cenc";
	uint8_t initDataTypeLen = strlen(initDataType);

	// Mock opencdm_construct_session to return success
	EXPECT_CALL(*g_mockopencdm, opencdm_construct_session(ocdmSystem, LicenseType::Temporary, 
		MemBufEq(initDataType, initDataTypeLen), MemBufEq(initData, initDataLen), 
		f_cbInitData, MemBufEq(customData.c_str(), customData.length()), 
		customData.length(), _, _, _))
		.WillOnce(Return(ERROR_NONE));

	// Call generateDRMSession
	m_ocdmsessionadapter->generateDRMSession(initData, f_cbInitData, customData);

	// Verify that KeyState is NOT set to error state
	KeyState state = m_ocdmsessionadapter->getState();
	EXPECT_NE(state, KEY_ERROR_SESSION_CONSTRUCT_FAILED);
	EXPECT_NE(state, KEY_ERROR);
}
