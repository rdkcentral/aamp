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

#include "AampOcdmBasicSessionAdapter.h"
#include "MockAampConfig.h"
#include "MockOpenCdmSessionAdapter.h"
#include "MockAampDrmHelper.h"
#include "MockOpenCdm.h"
#include "MockAampMemorySystem.h"
#include <cstring>


using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::SetArgReferee;
using ::testing::DoAll;

AampConfig *gpGlobalConfig{nullptr};

// For comparing memory buffers such as C-style arrays
MATCHER_P2(MemBufEq, buffer, elementCount, "")
{
	return std::memcmp(arg, buffer, elementCount * sizeof(buffer[0])) == 0;
}

std::shared_ptr<MockAampDrmHelper> drmHelper;
DrmInfo drminfo;
MockAampMemorySystem *g_mockAampMemorySystem;

class AampOcdmBasicSessionAdapterTests : public ::testing::Test
{

protected:
	AAMPOCDMBasicSessionAdapter *m_aampocdmbasicsessionadapter = nullptr;

	void SetUp() override
	{
		drmHelper = std::make_shared<MockAampDrmHelper>();
		g_mockopencdm = new NiceMock<MockOpenCdm>();
		m_aampocdmbasicsessionadapter = new AAMPOCDMBasicSessionAdapter(drmHelper,nullptr);
		g_mockOpenCdmSessionAdapter = new NiceMock<MockOpenCdmSessionAdapter>();
		g_mockAampMemorySystem = new NiceMock<MockAampMemorySystem>();
	}

	void TearDown() override
	{
		delete m_aampocdmbasicsessionadapter;
		m_aampocdmbasicsessionadapter = nullptr;

		delete g_mockOpenCdmSessionAdapter;
		g_mockOpenCdmSessionAdapter = nullptr;

		delete g_mockopencdm;
		g_mockopencdm = nullptr;

		drmHelper = nullptr;

		delete g_mockAampMemorySystem;
		g_mockAampMemorySystem = nullptr;
	}

public:
};

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptHDCPComplianceFailure)
{
	int ret_value;
	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(false));
	EXPECT_CALL(*g_mockopencdm, opencdm_session_decrypt(_,_,_,_,_,_,_,_,_,_)).Times(0);

	ret_value = m_aampocdmbasicsessionadapter->decrypt(nullptr, 0x0, nullptr,
										 0, nullptr);

	EXPECT_EQ(ret_value,HDCP_COMPLIANCE_CHECK_FAILURE);
}

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptWithNullMemorySystem)
{
	int ret_value;
	const uint8_t *f_pbIV = nullptr;
	uint32_t f_cbIV = 0;
	const uint8_t payloadData[] = {1,2,3};
	uint32_t payloadDataSize = sizeof(payloadData);
	EncryptionScheme encScheme = AesCtr_Cenc;
	uint32_t initWithLast15 = 0;

	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(true));
	EXPECT_CALL(*drmHelper, getMemorySystem()).WillRepeatedly(Return(nullptr));
	EXPECT_CALL(*g_mockopencdm,
				opencdm_session_decrypt(_,
										MemBufEq(payloadData, payloadDataSize),
										payloadDataSize,
										encScheme,
										_,
										f_pbIV,
										f_cbIV,
										MemBufEq(g_mockKeyId.data(), g_mockKeyId.size()),
										g_mockKeyId.size(),
										initWithLast15)).WillOnce(Return(ERROR_NONE));
	ret_value = m_aampocdmbasicsessionadapter->decrypt(f_pbIV, f_cbIV, payloadData,
										 payloadDataSize, nullptr);

	EXPECT_EQ(ret_value,0);
}

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptWithValidMemorySystem)
{
	int ret_value;
	const uint8_t f_pbIV[] = {12};
	uint32_t f_cbIV = 2;
	const uint8_t payloadData[] = {1,2,3};
	uint32_t payloadDataSize = sizeof(payloadData);
	EncryptionScheme encScheme = AesCtr_Cenc;
	uint32_t initWithLast15 = 0;
	std::vector<uint8_t> vdata {250, 3};
	uint32_t sizeToSend = vdata.size();
	uint8_t *dataToSend = vdata.data();

	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(true));
	EXPECT_CALL(*drmHelper, getMemorySystem()).WillRepeatedly(Return(g_mockAampMemorySystem));
	EXPECT_CALL(*g_mockAampMemorySystem, encode(payloadData,payloadDataSize,_)).WillOnce(DoAll(SetArgReferee<2>(vdata), Return(true)));
	EXPECT_CALL(*g_mockopencdm,
				opencdm_session_decrypt(_,
										MemBufEq(dataToSend, sizeToSend),
										sizeToSend,
										encScheme,
										_,
										f_pbIV,
										f_cbIV,
										MemBufEq(g_mockKeyId.data(), g_mockKeyId.size()),
										g_mockKeyId.size(),
										initWithLast15)).WillOnce(Return(ERROR_NONE));
	EXPECT_CALL(*g_mockAampMemorySystem, decode(MemBufEq(dataToSend, sizeToSend), sizeToSend,const_cast<uint8_t *>(payloadData), payloadDataSize)).WillOnce(Return(true));
	ret_value = m_aampocdmbasicsessionadapter->decrypt(f_pbIV, f_cbIV, payloadData,
										 payloadDataSize, nullptr);
	EXPECT_EQ(ret_value,0);
}

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptWithValidMemorySystemEncodeFail)
{
	int ret_value;
	const uint8_t f_pbIV[] = {12};
	uint32_t f_cbIV = 2;
	const uint8_t payloadData[] = {1,2,3};
	uint32_t payloadDataSize = sizeof(payloadData);
	EncryptionScheme encScheme = AesCtr_Cenc;
	uint32_t initWithLast15 = 0;
	std::vector<uint8_t> vdata {250, 3};
	uint32_t sizeToSend = vdata.size();
	uint8_t *dataToSend = vdata.data();

	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(true));
	EXPECT_CALL(*drmHelper, getMemorySystem()).WillRepeatedly(Return(g_mockAampMemorySystem));
	EXPECT_CALL(*g_mockAampMemorySystem, encode(payloadData,payloadDataSize,_)).WillOnce(DoAll(SetArgReferee<2>(vdata), Return(false)));
	EXPECT_CALL(*g_mockopencdm, opencdm_session_decrypt(_,_,_,_,_,_,_,_,_,_)).Times(0);
	EXPECT_CALL(*g_mockAampMemorySystem, decode(_,_,_,_)).Times(0);
	ret_value = m_aampocdmbasicsessionadapter->decrypt(f_pbIV, f_cbIV, payloadData,
										 payloadDataSize, nullptr);
	EXPECT_EQ(ret_value,-1);
}

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptWithValidMemorySystemDecodeFail)
{
	int ret_value;
	const uint8_t f_pbIV[] = {12};
	uint32_t f_cbIV = 2;
	const uint8_t payloadData[] = {1,2,3};
	uint32_t payloadDataSize = sizeof(payloadData);
	EncryptionScheme encScheme = AesCtr_Cenc;
	uint32_t initWithLast15 = 0;
	std::vector<uint8_t> vdata {250, 3};
	uint32_t sizeToSend = vdata.size();
	uint8_t *dataToSend = vdata.data();

	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(true));
	EXPECT_CALL(*drmHelper, getMemorySystem()).WillRepeatedly(Return(g_mockAampMemorySystem));
	EXPECT_CALL(*g_mockAampMemorySystem, encode(payloadData,payloadDataSize,_)).WillOnce(DoAll(SetArgReferee<2>(vdata), Return(true)));
	EXPECT_CALL(*g_mockopencdm,
				opencdm_session_decrypt(_,
										MemBufEq(dataToSend, sizeToSend),
										sizeToSend,
										encScheme,
										_,
										f_pbIV,
										f_cbIV,
										MemBufEq(g_mockKeyId.data(), g_mockKeyId.size()),
										g_mockKeyId.size(),
										initWithLast15)).WillOnce(Return(ERROR_NONE));
	EXPECT_CALL(*g_mockAampMemorySystem, decode(MemBufEq(dataToSend, sizeToSend), sizeToSend,const_cast<uint8_t *>(payloadData), payloadDataSize)).WillOnce(Return(false));							
	ret_value = m_aampocdmbasicsessionadapter->decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, nullptr);

	EXPECT_EQ(ret_value,-1);
}

TEST_F(AampOcdmBasicSessionAdapterTests, DecryptFail)
{
	int ret_value;
	const uint8_t f_pbIV[] = {12};
	uint32_t f_cbIV = 2;
	const uint8_t payloadData[] = {1,2,3};
	uint32_t payloadDataSize = sizeof(payloadData);
	EncryptionScheme encScheme = AesCtr_Cenc;
	uint32_t initWithLast15 = 0;
	std::vector<uint8_t> vdata {250, 3};
	uint32_t sizeToSend = vdata.size();
	uint8_t *dataToSend = vdata.data();

	EXPECT_CALL(*g_mockOpenCdmSessionAdapter, verifyOutputProtection()).WillOnce(Return(true));
	EXPECT_CALL(*drmHelper, getMemorySystem()).WillRepeatedly(Return(g_mockAampMemorySystem));
	EXPECT_CALL(*g_mockAampMemorySystem, encode(payloadData,payloadDataSize,_)).WillOnce(DoAll(SetArgReferee<2>(vdata), Return(true)));
	EXPECT_CALL(*g_mockopencdm,
				opencdm_session_decrypt(_,
										MemBufEq(dataToSend, sizeToSend),
										sizeToSend,
										encScheme,
										_,
										f_pbIV,
										f_cbIV,
										MemBufEq(g_mockKeyId.data(), g_mockKeyId.size()),
										g_mockKeyId.size(),
										initWithLast15)).WillOnce(Return(ERROR_UNKNOWN));
	EXPECT_CALL(*g_mockAampMemorySystem, terminateEarly());
	ret_value = m_aampocdmbasicsessionadapter->decrypt(f_pbIV, f_cbIV, payloadData, payloadDataSize, nullptr);
	EXPECT_EQ(ret_value,ERROR_UNKNOWN);
}
