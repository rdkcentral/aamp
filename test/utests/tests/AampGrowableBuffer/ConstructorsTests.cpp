/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
#include "MockGLib.h"
#include "AampGrowableBuffer.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

#include <functional>
#include <cmath>


using ::testing::NiceMock;
using ::testing::_;
using ::testing::Return;

class ConstructorsTests : public ::testing::Test
{
protected:
	ConstructorsTests()
	: data_buf(data_len)
	{
		callMalloc = [](size_t size){ return malloc(size); };
		callRealloc = [](gpointer ptr, size_t size){ return realloc(ptr, size); };
		callFree = [](gpointer ptr){ free(ptr); return; };
	}

	void SetUp() override
	{
		g_mockGLib = new NiceMock<MockGLib>();

		data_buf.reserve(data_len);
		for (auto & data : data_buf)
		{
			data = (char)rand();
		}
	}

	void TearDown() override
	{
		delete g_mockGLib;
	}

	std::vector<char> data_buf;
	static constexpr uint16_t data_len = 128;

public:
	std::function<gpointer (size_t)>callMalloc;
	std::function<gpointer (gpointer, size_t)>callRealloc;
	std::function<void (gpointer)>callFree;
};

TEST_F(ConstructorsTests, Copy)
{
	AampGrowableBuffer buf("buf-copyctor");

	EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillRepeatedly(callMalloc);
	buf.ReserveBytes(data_len);

	EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillRepeatedly(callRealloc);
	buf.AppendBytes(data_buf.data(), data_buf.size());

	auto tester = [this, &buf](AampGrowableBuffer & test_buf)
	{
		const auto * buf_ptr = buf.GetPtr();
		char * bufcopy_ptr = test_buf.GetPtr();

		EXPECT_NE(bufcopy_ptr, nullptr);
		EXPECT_NE(buf_ptr, bufcopy_ptr);
		EXPECT_EQ(buf.GetLen(), test_buf.GetLen());

		bufcopy_ptr[0] = (buf_ptr[0] + 1) & 0xff;

		EXPECT_NE(*bufcopy_ptr++, data_buf[0]);
		for (uint16_t idx = 1; idx < test_buf.GetLen(); idx++)
		{
			EXPECT_EQ(*bufcopy_ptr++, data_buf[idx]);
		}
	};
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

	// Copy constructor
	{
		AampGrowableBuffer buf_ctor{buf};
		tester(buf_ctor);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

	// Copy assignment
	{
		AampGrowableBuffer buf_assign("buf-copyassign");
		buf_assign = buf;
		tester(buf_assign);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).Times(2).WillRepeatedly(callFree);

	// Copy assignment with replacement
	{
		AampGrowableBuffer buf_assign("buf-copyreplacement");
		buf_assign.ReserveBytes(2*data_len);
		buf_assign.AppendBytes(&data_buf[0], data_buf.size());
		buf_assign = buf;
		tester(buf_assign);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);
}

TEST_F(ConstructorsTests, Move)
{
	AampGrowableBuffer buf("buf-move-ctor");

	EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillRepeatedly(callMalloc);
	buf.ReserveBytes(data_len);

	EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillRepeatedly(callRealloc);
	buf.AppendBytes(&data_buf[0], data_buf.size());

	auto tester = [this](const AampGrowableBuffer & src_buf, AampGrowableBuffer & test_buf)
	{
		const auto * buf_ptr = src_buf.GetPtr();
		char * bufcopy_ptr = test_buf.GetPtr();

		EXPECT_EQ(buf_ptr, nullptr);
		EXPECT_EQ(src_buf.GetLen(), 0);
		EXPECT_NE(buf_ptr, bufcopy_ptr);
		EXPECT_NE(src_buf.GetLen(), test_buf.GetLen());

		for (uint16_t idx = 0; idx < test_buf.GetLen(); idx++)
		{
			EXPECT_EQ(*bufcopy_ptr++, data_buf[idx]);
		}
	};
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

	// Move constructor
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_ctor{std::move(buf_copy)};
		tester(buf_copy, buf_ctor);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

	// Move assignment
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_assign("buf-moveassign");

		buf_assign = std::move(buf_copy);
		tester(buf_copy, buf_assign);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).Times(2).WillRepeatedly(callFree);

	// Move assignment with replacement
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_assign("buf-movereplacement");

		buf_assign.ReserveBytes(2*data_len);
		buf_assign.AppendBytes(&data_buf[0], data_buf.size());

		buf_assign = std::move(buf_copy);
		tester(buf_copy, buf_assign);
	}
	EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);
}
