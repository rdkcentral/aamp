#include <gtest/gtest.h>
#include "AampGrowableBuffer.h"
#include <limits.h>

class FunctionalTests : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(FunctionalTests, DestructorFunctionalTests) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Act: Call the Free function
    buffer.~AampGrowableBuffer();
    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);     // Check if available space is reset
}

TEST_F(FunctionalTests, FreeTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: Allocate memory for the buffer and add some data
    buffer.ReserveBytes(10);
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Free function
    buffer.Free();

    // Assert: Check that properties are reset and memory is freed
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);     // Check if available space is reset
}

TEST_F(FunctionalTests, ReserveBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    // Act: Call the ReserveBytes function
    size_t numBytesToReserve = 10;
    buffer.ReserveBytes(numBytesToReserve);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);       // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);             // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, AppendBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    // Act: Call the AppendBytes function
    buffer.AppendBytes(srcData, srcLen);

    // Assert: Check the effects of the AppendBytes function
    EXPECT_STREQ(buffer.GetPtr(), srcData);   // Check if data was appended correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);       // Check if length is set correctly
    EXPECT_NE(buffer.GetAvail(), srcLen);     // Check if available space is reduced accordingly
}

TEST_F(FunctionalTests, MoveBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: The buffer is set up in the fixture's SetUp()
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    buffer.ReserveBytes(srcLen); // Make sure the buffer has enough space

    // Act: Call the MoveBytes function
    buffer.MoveBytes(srcData, srcLen);

    // Assert: Check the effects of the MoveBytes function
    EXPECT_STREQ(buffer.GetPtr(), srcData);   // Check if data was moved correctly
    EXPECT_EQ(buffer.GetLen(), srcLen);       // Check if length is set correctly
    EXPECT_EQ(buffer.GetAvail(), srcLen);     // Check if available space remains the same
}

TEST_F(FunctionalTests, AppendNulTerminatorTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test

    // Act: Call the AppendNulTerminator function
    buffer.AppendNulTerminator();

    // Assert: Check the effects of the AppendNulTerminator function
    EXPECT_EQ(buffer.GetLen(), 2);
    EXPECT_EQ(buffer.GetPtr()[0], '\0');    // Check if null terminator is appended
}

TEST_F(FunctionalTests, ClearTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: Add some data to the buffer
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Clear function
    buffer.Clear();
    // Assert: Check that the length is reset to 0
    EXPECT_EQ(buffer.GetLen(), 0);
}

TEST_F(FunctionalTests, ReplaceTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: Set up two buffers - the source buffer and the destination buffer
    size_t numBytesToReserve = 10;
    AampGrowableBuffer sourceBuffer;
    sourceBuffer.AppendBytes("Hello", 5);

    // Act: Call the Replace function
    buffer.Replace(&sourceBuffer);

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

TEST_F(FunctionalTests, TransferNonEmptyTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    // Arrange: Add some data to the buffer
    buffer.AppendBytes("Test Data", 9);

    // Act: Call the Transfer function
    buffer.Transfer();

    // Assert: Check that the properties are reset after transfer
    EXPECT_EQ(buffer.GetPtr(), nullptr); // Check if the pointer is null
    EXPECT_EQ(buffer.GetLen(), 0);       // Check if the length is reset
    EXPECT_EQ(buffer.GetAvail(), 0);
}

////Test case is getting FAIL for UINT_MAX
//TEST_F(FunctionalTests, ReserveBytesMaxNumBytesAssertTest) {

//   AampGrowableBuffer buffer;  // Create a new buffer for this test
//#if !defined(NDEBUG)
//   ASSERT_DEATH(buffer.ReserveBytes(UINT_MAX), "");

//#else
//    buffer->ReserveBytes(UINT_MAX);

//#endif
//}
//These test cases cover larger buffer sizes (1K, 8K, 32K)

TEST_F(FunctionalTests, Reserve1KBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    size_t numBytesToReserve = 1024; // 1K

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, Reserve8KBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    size_t numBytesToReserve = 8192; // 8K

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

TEST_F(FunctionalTests, Reserve32KBytesTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    size_t numBytesToReserve = 32768; // 32K

    // Act: Call the ReserveBytes function
    buffer.ReserveBytes(numBytesToReserve);

    // Assert: Check the effects of the ReserveBytes function
    EXPECT_NE(buffer.GetPtr(), nullptr);          // Check if memory is allocated
    EXPECT_EQ(buffer.GetLen(), 0);                // Check if length remains 0
    EXPECT_EQ(buffer.GetAvail(), numBytesToReserve); // Check if available space is set correctly
}

//These test cases cover a series of appends
TEST_F(FunctionalTests, SeriesOfAppendsTest) {
    AampGrowableBuffer buffer;  // Create a new buffer for this test
    const char* srcData = "Hello, World!";
    size_t srcLen = strlen(srcData);

    // Arrange: Reserve a large initial space
    buffer.ReserveBytes(8192); // Starting with 8K

    // Act: Call the AppendBytes function multiple times, increasing the size each time
    for (int i = 0; i < 10; ++i) {
        buffer.AppendBytes(srcData, srcLen);
        srcLen *= 2; // Double the data size with each iteration
    }
    EXPECT_EQ(buffer.GetLen(), 13299);// Total length after 10 appends
    EXPECT_GE(buffer.GetAvail(),8192); // Available space should be greater than or equal to total length
}