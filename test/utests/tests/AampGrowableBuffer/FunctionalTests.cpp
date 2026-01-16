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
	GTEST_SKIP(); // invalid test - methods called after destructing
	
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    // Act: Call the Free function
    buffer.~AampGrowableBuffer();
    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
}

TEST_F(FunctionalTests, FreeTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test

    // Arrange: Allocate memory for the buffer and add some data
    // No g_malloc expectation needed - std::vector manages its own memory
    buffer.ReserveBytes(10);
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Free function
    // No g_free expectation needed - std::vector RAII handles cleanup
    buffer.Free();

    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
}

TEST_F(FunctionalTests, ReserveBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    // Act: Call the ReserveBytes function

    // No g_malloc expectation needed - std::vector manages its own memory
    size_t numBytesToReserve = 10;
    buffer.ReserveBytes(numBytesToReserve);

    // No g_free expectation needed - std::vector RAII handles cleanup

    // Assert: Check the effects of the ReserveBytes function
    // Buffer has reserved capacity but is still empty (size == 0)
    EXPECT_EQ(buffer.GetPtr(), nullptr);       // Buffer is empty, so pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);             // Check if length remains 0
    EXPECT_GE(buffer.GetAvail(), numBytesToReserve); // Capacity should be at least what we reserved
}

TEST_F(FunctionalTests, AppendBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
 
    // Arrange: The buffer is set up in the fixture's SetUp()
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    // No g_realloc expectation needed - std::vector manages its own memory

    // Act: Call the AppendBytes function
    buffer.AppendBytes(srcData, srcLen);

    // Assert: Check the effects of the AppendBytes function
    // These aren't null terminated strings, must use memcmp
    int result = memcmp(buffer.GetPtr(), srcData, srcLen);

    // No g_free expectation needed - std::vector RAII handles cleanup

    EXPECT_EQ(result, 0);                     // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);       // Check if length is set correctly
    // Note: GetAvail() no longer exists - std::vector manages capacity internally
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

    // No g_realloc expectation needed - std::vector manages its own memory
    sourceBuffer.AppendBytes("Hello", 5);

    // Act: Call the Replace function
    buffer.Replace(&sourceBuffer);

    // No g_free expectation needed - std::vector RAII handles cleanup

    // Assert: Check the effects of the Replace function on the destination buffer
    EXPECT_EQ(memcmp(buffer.GetPtr(), "Hello", 5), 0);
    EXPECT_EQ(buffer.GetLen(), 5);                    // Check if length is replaced
    // Note: GetAvail() no longer exists - std::vector manages capacity internally

    // Assert: Check the effects of the Replace function on the source buffer
    EXPECT_EQ(sourceBuffer.GetPtr(), nullptr); // Check if source pointer is reset
    EXPECT_EQ(sourceBuffer.GetLen(), 0);       // Check if source length is reset
}

TEST_F(FunctionalTests, ExtractNonEmptyTest)
{
    // Create a new buffer for this test
    AampGrowableBuffer buffer("buffer");

    // Arrange: Add some data to the buffer
    buffer.AppendBytes("Test Data", 9);

    // Store pointer and length before transfer (simulating what GStreamer does)
    const void* dataPtr = buffer.GetPtr();
    size_t dataLen = buffer.GetLen();
    
    // Verify data is present
    EXPECT_NE(dataPtr, nullptr);
    EXPECT_EQ(dataLen, 9);
    
    // Extract the internal vector and take ownership (returned by value, moved)
    std::vector<uint8_t> vec = buffer.ExtractVector();

    // Validate the returned vector contains the same data
    EXPECT_EQ(vec.size(), dataLen);
    EXPECT_EQ(memcmp(vec.data(), dataPtr, dataLen), 0);

    // No need to delete - vector is automatically cleaned up (RAII)

    // Assert: Check that the properties are reset after extraction
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if the pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if the length is reset
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

    // No g_malloc expectation needed - std::vector manages its own memory

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // No g_free expectation needed - std::vector RAII handles cleanup

    // Assert: Check the effects of the ReserveBytes function
    // With std::vector, reserve() allocates capacity but buffer remains empty
    EXPECT_EQ(buffer.GetPtr(), nullptr);          // Memory is allocated (capacity > 0)
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_GE(buffer.GetAvail(), numBytesToReserve); // Capacity should be at least what we reserved
}

TEST_F(FunctionalTests, Reserve8KBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    size_t numBytesToReserve = 8192; // 8K

    // No g_malloc expectation needed - std::vector manages its own memory

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // No g_free expectation needed - std::vector RAII handles cleanup


    // Assert: Check the effects of the ReserveBytes function
    EXPECT_EQ(buffer.GetPtr(), nullptr);          // Memory is allocated (capacity > 0)
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_GE(buffer.GetAvail(), numBytesToReserve); // Capacity should be at least what we reserved
}

TEST_F(FunctionalTests, Reserve32KBytesTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    size_t numBytesToReserve = 32768; // 32K

    // No g_malloc expectation needed - std::vector manages its own memory

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // No g_free expectation needed - std::vector RAII handles cleanup

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_EQ(buffer.GetPtr(), nullptr);          // Memory is allocated (capacity > 0)
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_GE(buffer.GetAvail(), numBytesToReserve); // Capacity should be at least what we reserved
}

// These test cases cover a series of appends
TEST_F(FunctionalTests, SeriesOfAppendsTest)
{
    AampGrowableBuffer buffer("buffer");  // Create a new buffer for this test
    const char srcData[8192] = "Hello, World!";
    size_t srcLen = strlen(srcData);

    // No g_realloc expectation needed - std::vector manages its own memory


    // Arrange: Reserve a large initial space
    buffer.ReserveBytes(8192); // Starting with 8K

    // Act: Call the AppendBytes function multiple times, increasing the size each time
    for (int i = 0; i < 10; ++i) {
        buffer.AppendBytes(srcData, srcLen);
        srcLen *= 2; // Double the data size with each iteration
    }

    // No g_free expectation needed - std::vector RAII handles cleanup

    EXPECT_EQ(buffer.GetLen(), 13299);// Total length after 10 appends
    // Note: GetAvail() no longer exists - std::vector automatically resizes
}

TEST_F(FunctionalTests, SetLenPositiveTest)
{
    AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);
    size_t srcNewLen = srcLen / 2;          // Reduce the length to half

    // No g_realloc expectation needed - std::vector manages its own memory

    // No g_free expectation needed - std::vector RAII handles cleanup

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
#ifdef __APPLE__
	GTEST_SKIP(); // avoid hang on OSX
#endif
	AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    {
        AampGrowableBuffer testBuf("testBuf");
        // No g_malloc expectation needed - std::vector manages its own memory
        testBuf.ReserveBytes(10);
        testBuf.SetLen(9);
        EXPECT_EQ(testBuf.GetLen(), 9);

        EXPECT_DEATH(testBuf.SetLen(11), _);
        EXPECT_EQ(testBuf.GetLen(), 9);
    }
}

TEST_F(FunctionalTests, SetLenAfterAppendBytesTest)
{
#ifdef __APPLE__ // avoid hang on OSX
	GTEST_SKIP();
#endif
    AampGrowableBuffer buffer("buffer");    // Create a new buffer for this test

    const char* srcData = "Hello, World";
    size_t srcLen = strlen(srcData);

    // No g_realloc expectation needed - std::vector manages its own memory

    // No g_free expectation needed - std::vector RAII handles cleanup

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
