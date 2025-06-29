/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
#include <cstring>
#include <functional>

#include "isobmff/isobmffhelper.h"
#include "AampConfig.h"
#include "MockGLib.h"
#include "testdata/testdata.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Eq;
using ::testing::_;
using ::testing::Address;
using ::testing::DoAll;
using ::testing::SetArgPointee;

AampConfig *gpGlobalConfig{nullptr};

class IsoBmffConvertToKeyFrameTests : public ::testing::Test
{
	protected:

		std::shared_ptr<IsoBmffHelper> helper;

		void SetUp() override
		{
			g_mockGLib = new NiceMock<MockGLib>();
			gpGlobalConfig = new AampConfig();
			helper = std::make_shared<IsoBmffHelper>();
		}

		void TearDown() override
		{
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
			delete g_mockGLib;
			g_mockGLib = nullptr;
		}
	public:

		IsoBmffConvertToKeyFrameTests()
		{
			callMalloc = [](size_t size){ return malloc(size); };
			callRealloc = [](gpointer ptr, size_t size){ return realloc(ptr, size); };
			callFree = [](gpointer ptr){ free(ptr); return; };
		}

		std::function<gpointer (size_t)>callMalloc;
		std::function<gpointer (gpointer, size_t)>callRealloc;
		std::function<void (gpointer)>callFree;
};

class IsoBmffConvertToKeyFrameTestsP : public IsoBmffConvertToKeyFrameTests,
	                                   public testing::WithParamInterface<test_data_t>
{
	public:
       struct PrintToStringParamName
       {
          template <class ParamType>
          std::string operator()( const testing::TestParamInfo<ParamType>& info ) const
          {
             auto test_data = static_cast<test_data_t>(info.param);
             return test_data.test_name;
          }
       };
};

void dumpCommonBytes(uint8_t* &actual, uint8_t* &expected, uint32_t &pos)
{
	char printable[17]={0};
	std::cout << "common:" << std::endl;
	std::cout << " 0x" << std::setw(8) << std::setfill('0') << std::hex << pos << ":";
	while (*actual == *expected)
	{
		std::cout << " 0x" << std::setw(2) << std::setfill('0') << std::hex  << unsigned(*expected);
		actual++;expected++;
		if (0 == (++pos % 16))
		{
			std::cout << "   " << printable << std::endl;
			std::cout << " 0x" << std::setw(8) << std::setfill('0') << std::hex << pos << ":";
		}
		printable[(pos % 16)] = (std::isprint(*expected) ? *expected : '.');
	}
	printable[(pos % 16)] = 0;
	std::cout << "   " << printable << std::endl;
	std::cout << std::endl;
}

void dumpBytes(uint8_t* &b_ptr, uint32_t num_bytes)
{
	char printable[17]={0};
	for(auto jj = 0; jj < num_bytes; jj++)
	{
		printable[jj] = (std::isprint(*b_ptr) ? *b_ptr: '.');
		std::cout << " 0x" << std::setw(2) << std::setfill('0') << std::hex  << (unsigned int)*b_ptr++;
	}
	std::cout << "   " << printable << std::endl;
}

TEST_P(IsoBmffConvertToKeyFrameTestsP, converToIFrame)
{
	AampGrowableBuffer src_data{"srcData"};

	EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillRepeatedly(callMalloc);
	EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillRepeatedly(callRealloc);

	test_data_t td = GetParam();
	src_data.AppendBytes(td.input_data,  td.input_data_len);

	EXPECT_TRUE(helper->ConvertToKeyFrame(src_data));
	EXPECT_EQ(src_data.GetLen(), td.expected_data_len);
	auto memcmp_actual_vs_expected = std::memcmp(src_data.GetPtr(), td.expected_data,  td.expected_data_len);
	EXPECT_EQ(0, memcmp_actual_vs_expected);
	if (memcmp_actual_vs_expected)
	{
		std::cout << "Result differs from expected!"  << std::endl;
		uint8_t* res = (uint8_t*)src_data.GetPtr();
		uint8_t* exp = td.expected_data;
		uint32_t ii = 0;
		dumpCommonBytes(res, exp, ii);

		std::cout << "Expected :";
		auto to_display = ((ii + 16) > td.expected_data_len) ? (td.expected_data_len - ii) : 16;
		dumpBytes(exp, to_display);
		std::cout << "Actual   :";
		dumpBytes(res, to_display);
	}

	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);
}

INSTANTIATE_TEST_SUITE_P(IsoBmffConvertToKeyFrameTests, IsoBmffConvertToKeyFrameTestsP, testing::ValuesIn(test_data), IsoBmffConvertToKeyFrameTestsP::PrintToStringParamName());
