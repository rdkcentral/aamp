/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file AampFragmentTestCases.cpp
 * @brief Unit test cases for AampFragment class
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include "AampFragment.h"
#include "AampTime.h"

/**
 * @brief Test fixture for AampFragment class
 */
class AampFragmentTest : public ::testing::Test
{
protected:
    /**
     * @brief Set up test fixtures before each test
     */
    void SetUp() override
    {
        // Setup code for each test
    }

    /**
     * @brief Clean up test fixtures after each test
     */
    void TearDown() override
    {
        // Cleanup code for each test
    }
};

/**
 * @brief Test that we can create a fragment with default constructor
 */
TEST_F(AampFragmentTest, DefaultConstructor_CreateFragment_EmptyUrl)
{
    // Arrange & Act
    AampFragment fragment;

    // Assert
    EXPECT_EQ(fragment.GetUrl(), "");
}

/**
 * @brief Test that we can create a fragment with URL constructor
 */
TEST_F(AampFragmentTest, UrlConstructor_CreateFragmentWithUrl_CorrectUrl)
{
    // Arrange
    const std::string testUrl = "http://example.com/fragment.m4s";

    // Act
    AampFragment fragment(testUrl);

    // Assert
    EXPECT_EQ(fragment.GetUrl(), testUrl);
}

/**
 * @brief Test that we can set and get URL
 */
TEST_F(AampFragmentTest, SetUrl_SetValidUrl_GetUrlReturnsCorrectValue)
{
    // Arrange
    AampFragment fragment;
    const std::string testUrl = "http://example.com/fragment.m4s";

    // Act
    fragment.SetUrl(testUrl);

    // Assert
    EXPECT_EQ(fragment.GetUrl(), testUrl);
}

/**
 * @brief Test that we can change URL
 */
TEST_F(AampFragmentTest, SetUrl_ChangeExistingUrl_GetUrlReturnsNewValue)
{
    // Arrange
    const std::string initialUrl = "http://example.com/fragment1.m4s";
    const std::string newUrl = "http://example.com/fragment2.m4s";
    AampFragment fragment(initialUrl);

    // Act
    fragment.SetUrl(newUrl);

    // Assert
    EXPECT_EQ(fragment.GetUrl(), newUrl);
}

/**
 * @brief Test URL with empty string
 */
TEST_F(AampFragmentTest, SetUrl_EmptyString_GetUrlReturnsEmptyString)
{
    // Arrange
    AampFragment fragment("http://example.com/fragment.m4s");

    // Act
    fragment.SetUrl("");

    // Assert
    EXPECT_EQ(fragment.GetUrl(), "");
}

/**
 * @brief Test URL with special characters
 */
TEST_F(AampFragmentTest, SetUrl_UrlWithSpecialCharacters_GetUrlReturnsCorrectValue)
{
    // Arrange
    AampFragment fragment;
    const std::string specialUrl = "https://example.com/path/to/fragment.m4s?param=value&time=123";

    // Act
    fragment.SetUrl(specialUrl);

    // Assert
    EXPECT_EQ(fragment.GetUrl(), specialUrl);
}

/**
 * @brief Test concurrent access to URL setter and getter methods
 * 
 * This test verifies that multiple threads can safely access the AampFragment
 * URL methods without causing data races or corruption.
 */
TEST_F(AampFragmentTest, ConcurrentAccess_MultipleThreadsSetAndGetUrl_ThreadSafe)
{
    // Arrange
    AampFragment fragment;
    const int numThreads = 10;
    const int operationsPerThread = 100;
    std::atomic<int> successfulOperations{0};
    std::vector<std::thread> threads;

    // Act - Launch multiple threads that set and get URLs concurrently
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&fragment, &successfulOperations, operationsPerThread, i]()
        {
            for (int j = 0; j < operationsPerThread; ++j)
            {
                try
                {
                    // Set a unique URL for this thread and operation
                    std::string url = "https://thread" + std::to_string(i) + "_op" + std::to_string(j) + ".com/fragment.m4s";
                    fragment.SetUrl(url);
                    
                    // Read back the URL (may not be the same due to concurrent access)
                    std::string retrievedUrl = fragment.GetUrl();
                    
                    // Verify that the retrieved URL is valid (not corrupted)
                    if (!retrievedUrl.empty() && retrievedUrl.find("https://") == 0)
                    {
                        successfulOperations++;
                    }
                }
                catch (...)
                {
                    // Thread safety violation detected
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Assert - All operations should complete without crashes or corruption
    // We expect at least some successful operations (exact count may vary due to race conditions)
    EXPECT_GT(successfulOperations.load(), 0);
    
    // Final URL should be valid
    std::string finalUrl = fragment.GetUrl();
    if (!finalUrl.empty())
    {
        EXPECT_TRUE(finalUrl.find("https://") == 0) << "Final URL appears corrupted: " << finalUrl;
    }
}

/**
 * @brief Test concurrent read-only access to URL getter method
 * 
 * This test verifies that multiple threads can safely read the URL
 * simultaneously without causing issues.
 */
TEST_F(AampFragmentTest, ConcurrentRead_MultipleThreadsGetUrl_ThreadSafe)
{
    // Arrange
    AampFragment fragment;
    const std::string testUrl = "https://readonly-test.com/fragment.m4s";
    fragment.SetUrl(testUrl);
    
    const int numReaderThreads = 20;
    const int readsPerThread = 500;
    std::atomic<int> successfulReads{0};
    std::atomic<int> correctReads{0};
    std::vector<std::thread> threads;

    // Act - Launch multiple reader threads
    for (int i = 0; i < numReaderThreads; ++i)
    {
        threads.emplace_back([&fragment, &successfulReads, &correctReads, readsPerThread, &testUrl]()
        {
            for (int j = 0; j < readsPerThread; ++j)
            {
                try
                {
                    std::string retrievedUrl = fragment.GetUrl();
                    successfulReads++;
                    
                    if (retrievedUrl == testUrl)
                    {
                        correctReads++;
                    }
                }
                catch (...)
                {
                    // Thread safety violation detected
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Assert - All reads should be successful and return correct value
    int expectedReads = numReaderThreads * readsPerThread;
    EXPECT_EQ(successfulReads.load(), expectedReads) << "Some read operations failed";
    EXPECT_EQ(correctReads.load(), expectedReads) << "Some reads returned incorrect values";
}

/**
 * @brief Test rapid concurrent modifications of URL
 * 
 * This test verifies that rapid concurrent modifications don't cause
 * memory corruption or undefined behavior.
 */
TEST_F(AampFragmentTest, RapidConcurrentModification_HighFrequencySetUrl_NoCorruption)
{
    // Arrange
    AampFragment fragment;
    const int numWriterThreads = 5;
    const int numReaderThreads = 5;
    const std::chrono::milliseconds testDuration{100}; // 100ms stress test
    std::atomic<bool> stopFlag{false};
    std::atomic<int> writeOperations{0};
    std::atomic<int> readOperations{0};
    std::atomic<int> corruptedReads{0};
    std::vector<std::thread> threads;

    // Act - Launch writer threads
    for (int i = 0; i < numWriterThreads; ++i)
    {
        threads.emplace_back([&fragment, &stopFlag, &writeOperations, i]()
        {
            int operationCount = 0;
            while (!stopFlag.load())
            {
                std::string url = "https://writer" + std::to_string(i) + "_" + std::to_string(operationCount++) + ".com/fragment.m4s";
                fragment.SetUrl(url);
                writeOperations++;
                std::this_thread::yield(); // Allow other threads to run
            }
        });
    }

    // Launch reader threads
    for (int i = 0; i < numReaderThreads; ++i)
    {
        threads.emplace_back([&fragment, &stopFlag, &readOperations, &corruptedReads]()
        {
            while (!stopFlag.load())
            {
                try
                {
                    std::string url = fragment.GetUrl();
                    readOperations++;
                    
                    // Check for obvious corruption (partial writes, invalid characters, etc.)
                    if (!url.empty())
                    {
                        // URL should be valid if not empty
                        if (url.find("https://") != 0 || 
                            url.find('\0') != std::string::npos ||
                            url.length() > 1000) // Reasonable max length
                        {
                            corruptedReads++;
                        }
                    }
                }
                catch (...)
                {
                    corruptedReads++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Let threads run for the test duration
    std::this_thread::sleep_for(testDuration);
    stopFlag.store(true);

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Assert - No corrupted reads should occur
    EXPECT_EQ(corruptedReads.load(), 0) << "Detected " << corruptedReads.load() << " corrupted reads out of " << readOperations.load() << " total reads";
    EXPECT_GT(writeOperations.load(), 0) << "No write operations were performed";
    EXPECT_GT(readOperations.load(), 0) << "No read operations were performed";
}

// === Fragment Caching Tests ===

/**
 * @brief Test setting and getting fragment data for complete fragments
 */
TEST_F(AampFragmentTest, FragmentData_SetCompleteFragment_DataStoredCorrectly)
{
    // Arrange
    AampFragment fragment;
    const uint8_t testData[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    const size_t testSize = sizeof(testData);

    // Act
    fragment.SetFragmentData(testData, testSize, AampFragment::COMPLETE_FRAGMENT);

    // Assert
    EXPECT_EQ(fragment.GetFragmentSize(), testSize);
    const uint8_t* retrievedData = fragment.GetFragmentData();
    ASSERT_NE(retrievedData, nullptr);
    EXPECT_EQ(std::memcmp(retrievedData, testData, testSize), 0);
    EXPECT_TRUE(fragment.IsComplete());
}

/**
 * @brief Test adding chunks to fragment (chunk-based fragments)
 */
TEST_F(AampFragmentTest, FragmentChunks_AddMultipleChunks_ChunksAccumulatedCorrectly)
{
    // Arrange
    AampFragment fragment;
    const uint8_t chunk1[] = {0x01, 0x02, 0x03};
    const uint8_t chunk2[] = {0x04, 0x05, 0x06};
    const uint8_t chunk3[] = {0x07, 0x08};

    // Act
    fragment.AddChunk(chunk1, sizeof(chunk1));
    fragment.AddChunk(chunk2, sizeof(chunk2));
    fragment.AddChunk(chunk3, sizeof(chunk3));

    // Assert
    EXPECT_EQ(fragment.GetChunkCount(), 3);
    EXPECT_EQ(fragment.GetFragmentSize(), sizeof(chunk1) + sizeof(chunk2) + sizeof(chunk3));
    
    // Verify accumulated data
    const uint8_t expectedData[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    const uint8_t* retrievedData = fragment.GetFragmentData();
    ASSERT_NE(retrievedData, nullptr);
    EXPECT_EQ(std::memcmp(retrievedData, expectedData, sizeof(expectedData)), 0);
}

/**
 * @brief Test fragment position and duration properties
 */
TEST_F(AampFragmentTest, FragmentMetadata_SetPositionAndDuration_MetadataStoredCorrectly)
{
    // Arrange
    AampFragment fragment;
    const AampTime testPosition(12.5);
    const AampTime testDuration(6.0);

    // Act
    fragment.SetPosition(testPosition);
    fragment.SetDuration(testDuration);

    // Assert
    EXPECT_EQ(fragment.GetPosition(), testPosition);
    EXPECT_EQ(fragment.GetDuration(), testDuration);
}

/**
 * @brief Test fragment flags (init fragment, discontinuity)
 */
TEST_F(AampFragmentTest, FragmentFlags_SetInitAndDiscontinuity_FlagsStoredCorrectly)
{
    // Arrange
    AampFragment fragment;

    // Act & Assert - Default values
    EXPECT_FALSE(fragment.IsInitFragment());
    EXPECT_FALSE(fragment.HasDiscontinuity());

    // Act - Set flags
    fragment.SetInitFragment(true);
    fragment.SetDiscontinuity(true);

    // Assert - Flags are set
    EXPECT_TRUE(fragment.IsInitFragment());
    EXPECT_TRUE(fragment.HasDiscontinuity());

    // Act - Clear flags
    fragment.SetInitFragment(false);
    fragment.SetDiscontinuity(false);

    // Assert - Flags are cleared
    EXPECT_FALSE(fragment.IsInitFragment());
    EXPECT_FALSE(fragment.HasDiscontinuity());
}

/**
 * @brief Test fragment clear functionality
 */
TEST_F(AampFragmentTest, Clear_FragmentWithData_AllDataCleared)
{
    // Arrange
    AampFragment fragment("https://example.com/fragment.m4s");
    const uint8_t testData[] = {0x01, 0x02, 0x03};
    fragment.SetFragmentData(testData, sizeof(testData));
    fragment.SetPosition(AampTime(10.0));
    fragment.SetDuration(AampTime(5.0));
    fragment.SetInitFragment(true);
    fragment.SetDiscontinuity(true);

    // Act
    fragment.Clear();

    // Assert
    EXPECT_EQ(fragment.GetUrl(), "");
    EXPECT_EQ(fragment.GetFragmentSize(), 0);
    EXPECT_EQ(fragment.GetChunkCount(), 0);
    EXPECT_EQ(fragment.GetPosition(), AampTime(0.0));
    EXPECT_EQ(fragment.GetDuration(), AampTime(0.0));
    EXPECT_FALSE(fragment.IsInitFragment());
    EXPECT_FALSE(fragment.HasDiscontinuity());
    EXPECT_FALSE(fragment.IsComplete());
}

/**
 * @brief Test copying fragment data from another fragment
 */
TEST_F(AampFragmentTest, CopyFrom_SourceFragmentWithData_DataCopiedCorrectly)
{
    // Arrange
    AampFragment sourceFragment("https://source.com/fragment.m4s");
    const uint8_t testData[] = {0xAA, 0xBB, 0xCC, 0xDD};
    sourceFragment.SetFragmentData(testData, sizeof(testData));
    sourceFragment.SetPosition(AampTime(15.5));
    sourceFragment.SetDuration(AampTime(4.0));
    sourceFragment.SetInitFragment(true);

    AampFragment targetFragment;

    // Act
    targetFragment.CopyFrom(sourceFragment);

    // Assert
    EXPECT_EQ(targetFragment.GetUrl(), sourceFragment.GetUrl());
    EXPECT_EQ(targetFragment.GetFragmentSize(), sourceFragment.GetFragmentSize());
    EXPECT_EQ(targetFragment.GetPosition(), sourceFragment.GetPosition());
    EXPECT_EQ(targetFragment.GetDuration(), sourceFragment.GetDuration());
    EXPECT_EQ(targetFragment.IsInitFragment(), sourceFragment.IsInitFragment());
    
    // Verify data content
    const uint8_t* sourceData = sourceFragment.GetFragmentData();
    const uint8_t* targetData = targetFragment.GetFragmentData();
    EXPECT_EQ(std::memcmp(sourceData, targetData, sizeof(testData)), 0);
}

/**
 * @brief Test thread safety of fragment caching operations
 */
TEST_F(AampFragmentTest, FragmentCaching_ConcurrentChunkOperations_ThreadSafe)
{
    // Arrange
    AampFragment fragment;
    const int numThreads = 5;
    const int chunksPerThread = 20;
    std::atomic<int> chunksAdded{0};
    std::vector<std::thread> threads;

    // Act - Multiple threads adding chunks simultaneously
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&fragment, &chunksAdded, chunksPerThread, i]()
        {
            for (int j = 0; j < chunksPerThread; ++j)
            {
                uint8_t chunkData[] = {static_cast<uint8_t>(i), static_cast<uint8_t>(j)};
                fragment.AddChunk(chunkData, sizeof(chunkData));
                chunksAdded++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }

    // Assert
    int expectedChunks = numThreads * chunksPerThread;
    EXPECT_EQ(chunksAdded.load(), expectedChunks);
    EXPECT_EQ(fragment.GetChunkCount(), expectedChunks);
    EXPECT_EQ(fragment.GetFragmentSize(), expectedChunks * 2); // 2 bytes per chunk
}
