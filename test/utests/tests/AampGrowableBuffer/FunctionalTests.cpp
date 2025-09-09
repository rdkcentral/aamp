/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "AampGrowableBuffer.h"
#include <limits.h>
#include <functional>
#include "MockGLib.h"
#include "AampLogManager.h"

using ::testing::NiceMock;
using ::testing::_;
using ::testing::Return;

class FunctionalTests : public ::testing::Test {
protected:
    FunctionalTests()
    {
        callMalloc = [](size_t size){ return malloc(size); };
        callRealloc = [](gpointer ptr, size_t size){ return realloc(ptr, size); };
        callFree = [](gpointer ptr){ free(ptr); return; };
    }

    void SetUp() override
    {
        g_mockGLib = new NiceMock<MockGLib>();
    }

    void TearDown() override
    {
        delete g_mockGLib;
        g_mockGLib = nullptr;
    }

public:
	std::function<gpointer (size_t)>callMalloc;
	std::function<gpointer (gpointer, size_t)>callRealloc;
	std::function<void (gpointer)>callFree;
};

TEST_F(FunctionalTests, DestructorFunctionalTests)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    // Act: Call the Free function
    buffer.~AampGrowableBuffer();
    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);     // Check if available space is reset
}

TEST_F(FunctionalTests, FreeTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);

    // Arrange: Allocate memory for the buffer and add some data
    buffer.ReserveBytes(10);
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Free function
    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);
    buffer.Free();

    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);     // Check if available space is reset
}

TEST_F(FunctionalTests, ReserveBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    // Act: Call the ReserveBytes function

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);

    size_t numBytesToReserve = 10;
    buffer.ReserveBytes(numBytesToReserve);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);       // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);             // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, AppendBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
 
    // Arrange: The buffer is set up in the fixture's SetUp()
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillOnce(callRealloc);

    // Act: Call the AppendBytes function
    buffer.AppendBytes(srcData, srcLen);

    // Assert: Check the effects of the AppendBytes function
    // These aren't null terminated strings, must use memcmp
    int result = memcmp(buffer.GetPtr(), srcData, srcLen);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    EXPECT_EQ(result, 0);                     // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);       // Check if length is set correctly
    EXPECT_NE(buffer.GetAvail(), srcLen);     // Check if available space is reduced accordingly
}

TEST_F(FunctionalTests, MoveBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);
 
    buffer.ReserveBytes(srcLen); // Make sure the buffer has enough space

    // Act: Call the MoveBytes function
    buffer.MoveBytes(srcData, srcLen);

    // Assert: Check the effects of the MoveBytes function
    // These aren't null terminated strings, must use memcmp
    int result = memcmp(buffer.GetPtr(), srcData, srcLen);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    EXPECT_EQ(result, 0);                     // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);       // Check if length is set correctly
    EXPECT_EQ(buffer.GetAvail(), srcLen);     // Check if available space remains the same
}

TEST_F(FunctionalTests, ClearTest)
{
    // Create a new buffer for this test
    AampGrowableBuffer buffer("buffer");

    // Arrange: Add some data to the buffer
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Clear function
    buffer.Clear();

    // Assert: Check that the length is reset to 0
    EXPECT_EQ(buffer.GetLen(), 0);
}

TEST_F(FunctionalTests, ReplaceTest)
{
    // Create a new buffer for this test
    AampGrowableBuffer buffer("buffer");

    // Arrange: Set up two buffers - the source buffer and the destination buffer
    AampGrowableBuffer sourceBuffer("buffer");

    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillOnce(callRealloc);

    sourceBuffer.AppendBytes("Hello", 5);

    // Act: Call the Replace function
    buffer.Replace(&sourceBuffer);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    // Assert: Check the effects of the Replace function on the destination buffer
    // EXPECT_EQ(buffer->GetPtr(), sourceBuffer.GetPtr()); // Check if pointer is replaced
    EXPECT_EQ(memcmp(buffer.GetPtr(), "Hello", 5), 0);
    EXPECT_EQ(buffer.GetLen(), 5);                    // Check if length is replaced
    EXPECT_EQ(buffer.GetAvail(), 10); // Check if available space is replaced

    // // Assert: Check the effects of the Replace function on the source buffer
    EXPECT_EQ(sourceBuffer.GetPtr(), nullptr); // Check if source pointer is reset
    EXPECT_EQ(sourceBuffer.GetLen(), 0);       // Check if source length is reset
    EXPECT_EQ(sourceBuffer.GetAvail(), 0);     // Check if source available space is reset
}

TEST_F(FunctionalTests, TransferNonEmptyTest)
{
    // Create a new buffer for this test
    AampGrowableBuffer buffer("buffer");

    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillOnce(callRealloc);

    // Arrange: Add some data to the buffer
    buffer.AppendBytes("Test Data", 9);

    //Temp store becase Buffer.transfer Nulls Pointer so cant be freed
    gpointer ptr {buffer.GetPtr()};

    // Act: Call the Transfer function
    buffer.Transfer();

    //
    free(ptr);
    // Assert: Check that the properties are reset after transfer
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if the pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if the length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);
}

////Test case is getting FAIL for UINT_MAX
//TEST_F(FunctionalTests, ReserveBytesMaxNumBytesAssertTest) {

//   AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
//#if !defined(NDEBUG)
//   ASSERT_DEATH(buffer.ReserveBytes(UINT_MAX), "");

//#else
//    buffer->ReserveBytes(UINT_MAX);

//#endif
//}

// These test cases cover larger buffer sizes (1K, 8K, 32K)
TEST_F(FunctionalTests, Reserve1KBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    size_t numBytesToReserve = 1024; // 1K

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, Reserve8KBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    size_t numBytesToReserve = 8192; // 8K

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);


    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, Reserve32KBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    size_t numBytesToReserve = 32768; // 32K

    EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

// These test cases cover a series of appends
TEST_F(FunctionalTests, SeriesOfAppendsTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    const char srcData[8192] = "Hello, World!";
    size_t srcLen = strlen(srcData);

    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillRepeatedly(callRealloc);


    // Arrange: Reserve a large initial space
    buffer.ReserveBytes(8192); // Starting with 8K

    // Act: Call the AppendBytes function multiple times, increasing the size each time
    for (int i = 0; i < 10; ++i) {
        buffer.AppendBytes(srcData, srcLen);
        srcLen *= 2; // Double the data size with each iteration
    }

    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    EXPECT_EQ(buffer.GetLen(), 13299);// Total length after 10 appends
    EXPECT_GE(buffer.GetAvail(),8192); // Available space should be greater than or equal to total length
}

TEST_F(FunctionalTests, SetLenPositiveTest)
{
    AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);
    size_t srcNewLen = srcLen / 2;          // Reduce the length to half

    // Expectation for AppendBytes()
    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillOnce(callRealloc);

    // Expectation for the destructor ~AampGrowableBuffer()
    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    buffer.AppendBytes(srcData, srcLen);

    // Assert: Check the effects of the AppendBytes function
    // These aren't null terminated strings, must use memcmp
    int result = memcmp(buffer.GetPtr(), srcData, srcLen);

    EXPECT_EQ(result, 0);                   // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);     // Check if length is set correctly

    buffer.SetLen(srcNewLen);
    EXPECT_EQ(buffer.GetLen(), srcNewLen);
}

TEST_F(FunctionalTests, SetLenAfterReserveBytesTest)
{
    AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    {
        AampGrowableBuffer testBuf("testBuf");
        EXPECT_CALL(*g_mockGLib, g_malloc(_)).WillOnce(callMalloc);
        testBuf.ReserveBytes(10);
        testBuf.SetLen(9);
        EXPECT_EQ(testBuf.GetLen(), 9);

        EXPECT_DEATH(testBuf.SetLen(11), _);
        EXPECT_EQ(testBuf.GetLen(), 9);
    }
}

TEST_F(FunctionalTests, SetLenAfterAppendBytesTest)
{
    AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    const char* srcData = "Hello, World";
    size_t srcLen = strlen(srcData);

    // Expectation for AppendBytes()
    EXPECT_CALL(*g_mockGLib, g_realloc(_,_)).WillOnce(callRealloc);

    // Expectation for the destructor ~AampGrowableBuffer()
    EXPECT_CALL(*g_mockGLib, g_free(_)).WillOnce(callFree);

    buffer.AppendBytes(srcData, srcLen);

    // Assert: Check the effects of the AppendBytes function
    // These aren't null terminated strings, must use memcmp
    int result = memcmp(buffer.GetPtr(), srcData, srcLen);

    EXPECT_EQ(result, 0);                   // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);     // Check if length is set correctly

    // attempt to set length bigger than available size
    EXPECT_DEATH(buffer.SetLen(100), _);
    EXPECT_EQ(buffer.GetLen(), srcLen);     // Check that length has not changed
}