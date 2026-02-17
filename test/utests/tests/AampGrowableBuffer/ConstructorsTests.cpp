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

		// Fill data_buf with random data (vector already sized to data_len in constructor)
		for (size_t i = 0; i < data_buf.size(); ++i)
		{
			data_buf[i] = (char)rand();
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

// Out-of-class definition required for C++11 when ODR-used
constexpr uint16_t ConstructorsTests::data_len;

TEST_F(ConstructorsTests, Copy)
{
	AampGrowableBuffer buf("buf-copyctor");

	// Reserve space and append data - no g_malloc expectations needed with std::vector
	buf.ReserveBytes(data_len);
	buf.AppendBytes(data_buf.data(), data_buf.size());

	// Tester validates that copy is independent and contains correct data
	auto tester = [this, &buf](AampGrowableBuffer & test_buf)
	{
		const auto * buf_ptr = buf.GetPtr();
		char * bufcopy_ptr = test_buf.GetPtr();

		// Verify the copy has valid data and correct length
		EXPECT_NE(bufcopy_ptr, nullptr);
		EXPECT_EQ(buf.size(), test_buf.size());

		// Modify first byte of copy to verify independence
		const char original_first_byte = bufcopy_ptr[0];
		bufcopy_ptr[0] = (buf_ptr[0] + 1) & 0xff;

		// Verify original is unchanged (proves independence)
		EXPECT_EQ(buf_ptr[0], static_cast<char>(data_buf[0]));
		
		// Verify copy was modified
		EXPECT_NE(bufcopy_ptr[0], static_cast<char>(data_buf[0]));
		
		// Verify rest of data matches
		EXPECT_EQ(memcmp(bufcopy_ptr + 1, data_buf.data() + 1, test_buf.size() - 1), 0);
	};

	// Copy constructor - std::vector RAII handles cleanup
	{
		AampGrowableBuffer buf_ctor{buf};
		tester(buf_ctor);
	}

	// Copy assignment
	{
		AampGrowableBuffer buf_assign("buf-copyassign");
		buf_assign = buf;
		tester(buf_assign);
	}

	// Copy assignment with replacement
	{
		AampGrowableBuffer buf_assign("buf-copyreplacement");
		buf_assign.ReserveBytes(2*data_len);
		buf_assign.AppendBytes(&data_buf[0], data_buf.size());
		buf_assign = buf;
		tester(buf_assign);
	}
}

TEST_F(ConstructorsTests, Move)
{
	AampGrowableBuffer buf("buf-move-ctor");

	// Reserve space and append data - no g_malloc expectations needed
	buf.ReserveBytes(data_len);
	buf.AppendBytes(&data_buf[0], data_buf.size());

	// Tester validates that move leaves source empty and transfers data correctly
	auto tester = [this](const AampGrowableBuffer & src_buf, AampGrowableBuffer & test_buf)
	{
		const auto * buf_ptr = src_buf.GetPtr();
		char * bufcopy_ptr = test_buf.GetPtr();

		// After move, source should be empty
		EXPECT_EQ(buf_ptr, nullptr);
		EXPECT_EQ(src_buf.size(), 0);
		
		// Destination should have the data
		EXPECT_NE(bufcopy_ptr, nullptr);
		EXPECT_EQ(test_buf.size(), data_len);

		// Verify data was transferred correctly
		EXPECT_EQ(memcmp(bufcopy_ptr, data_buf.data(), test_buf.size()), 0);
	};

	// Move constructor - std::vector RAII handles cleanup
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_ctor{std::move(buf_copy)};
		tester(buf_copy, buf_ctor);
	}

	// Move assignment
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_assign("buf-moveassign");

		buf_assign = std::move(buf_copy);
		tester(buf_copy, buf_assign);
	}

	// Move assignment with replacement
	{
		AampGrowableBuffer buf_copy{buf};
		AampGrowableBuffer buf_assign("buf-movereplacement");

		buf_assign.ReserveBytes(2*data_len);
		buf_assign.AppendBytes(&data_buf[0], data_buf.size());

		buf_assign = std::move(buf_copy);
		tester(buf_copy, buf_assign);
	}
}
